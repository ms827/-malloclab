/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void remove_free_list(void *bp);
static void insert_to_free_lst(void *bp);
static int find_idx(size_t asize);

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*초기 가용 리스트 조작을 위한 기본 상수 및 매크로 정의*/

#define WSIZE  4    
#define DSIZE  8    
#define NUM 8          // Segregated free list 배열 index 수
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x)>(y)? (x):(y))  

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)    (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)    ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)    ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PRED(bp)    (*(char **)(bp))            // 앞 블록의 주소(bp)
#define SUCC(bp)    (*(char **)(bp + WSIZE))    // 이전 블록의 주소(bp)


static char *heap_listp;                        // heap의 첫번째 포인터
static char *free_array[NUM];                   // 사이즈별로 클래스가 만들어질 Segregated free list 


int mm_init(void)
{
    for (int i=0; i < NUM; i++) {     // free_array의 루트들을 NULL로 초기화
      free_array[i] = NULL;
    }   
    /*creat'e the initial empty heap*/
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
      return -1;
    PUT(heap_listp, 0); /*Alignement padding, heap_listp 가 가르키는 위치에 0 삽입*/
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); /*PUT(주소, 값)Prologue header*/
    PUT(heap_listp + (2*WSIZE), PACK(2*DSIZE, 1)); //PRED(predecessor)
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); //SUCC(successor)
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
      return -1;

    return 0;
}

/* 
 * extend_heap - 2가지 경우에 호출
 1.힙이 초기화될 때
   초기화 후에 초기 가용 블록을 생성하기 위해 호출
 2.요청한 크기의 메모리 할당을 위해 충분한 공간을 찾지 못했을 때
   Double word 정렬을 위해 8의 배수로 반올림하고, 추가 힙 공간을 요청
 */
static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;

  size = (words %2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;

  PUT(HDRP(bp), PACK(size,0));
  PUT(FTRP(bp), PACK(size,0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

  return coalesce(bp);
}


void mm_free(void *bp)
{
  size_t size = GET_SIZE(HDRP(bp));     // 해당 블록의 size를 알아내 header와 footer의 정보를 수정한다.

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);                         // 만약 앞뒤의 블록이 가용 상태라면 연결한다.
}


static void *coalesce(void *bp)
{

  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));       // 이전 footer 할당 여부
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));       // 다음 header 할당 여부
  size_t size = GET_SIZE(HDRP(bp));                         // 현재 사이즈

  if (prev_alloc && next_alloc){                            // case1-앞 뒤 모두 할당 상태
    insert_to_free_lst(bp);                                 // free_array에 블록 삽입
    return bp;
  }

  else if (prev_alloc && !next_alloc){                      // case2-앞 할당 뒤 가용 상태
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));                  // 가용상태 블록과 합쳐준다
    remove_free_list(NEXT_BLKP(bp));                        // free_array 뒷 블럭 삭제
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
  }

  else if (!prev_alloc && next_alloc){                      // case3-앞 가용 뒤 할당 상태
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));                  // 가용상태 블록과 합쳐준다
    remove_free_list(PREV_BLKP(bp));                        // free_array 앞 블럭 삭제
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    bp = PREV_BLKP(bp);                                     // bp 갱신
  }

  else {                                                    // case3-앞 뒤 모두 가용 상태
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    remove_free_list(NEXT_BLKP(bp));
    remove_free_list(PREV_BLKP(bp));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));    
    bp = PREV_BLKP(bp);    
  }
  insert_to_free_lst(bp);                                   // free_array에 블록 삽입
  return bp;
}


void *mm_malloc(size_t size)
{
  size_t asize;
  size_t extendsize;
  char *bp;

  if (size ==0)
    return NULL;

  if (size <= DSIZE)          // 2words 이하의 사이즈는 4워드로 할당 요청 (header 1word, footer 1word)
    asize = 2*DSIZE;
  else                        // 할당 요청의 용량이 2words 초과 시, 충분한 8byte의 배수의 용량 할당
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

  if ((bp = find_fit(asize)) != NULL){      // 적당한 크기의 가용 블록 검색
    place(bp, asize);                       // 초과 부분을 분할하고 새롭게 할당한 블록의 포인터 반환
    return bp;
  }

  extendsize = MAX(asize, CHUNKSIZE);

  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;

  place(bp, asize);
  return bp;
}


void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


static void *find_fit(size_t asize) 
{
    void* bp;
    
    for (int i = find_idx(asize); i < NUM; i++){        // free_array에서 블럭이 속한 idx를 찾는다
      if(free_array[i]==NULL){
        continue;
      }
      for(bp=free_array[i]; bp != NULL; bp=SUCC(bp)){   // free_array[i]를 시작점으로, bp가 NULL이 아닐 때까지 탐색
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
  }
    return NULL; 
}


static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));      // 현재 할당할 수 있는 후보 가용 블록의 주소
  remove_free_list(bp);

  

  
  if ((csize-asize) >= (2*DSIZE)) {       // 분할이 가능한 경우
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));
    insert_to_free_lst(bp);

  }
  else {                                  // 분할이 필요없는 경우
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));

  }
}


static void remove_free_list(void *bp)
{
  int idx = find_idx(GET_SIZE(HDRP(bp)));


  if (free_array[idx] != bp){   
    if (SUCC(bp) != NULL) {     // 중간블록 remove
      SUCC(PRED(bp)) = SUCC(bp);
      PRED(SUCC(bp)) = PRED(bp);
      }
    else{                       // 맨 뒷 블럭 remove
      SUCC(PRED(bp)) = NULL;
    }
  }
  else{                        // 맨앞 블록 remove
    if (SUCC(bp) != NULL){     // 다음 블록이 존재하는 경우
      PRED(SUCC(bp)) = free_array[idx];
      free_array[idx] = SUCC(bp);
    } 
    else{                      // 블록이 단 하나
      free_array[idx] = NULL;
    }
  }
}

/* free_array 에 블럭 사이즈에 따라 알맞은 idx 위치로 가서 LIFO 구조로 블럭 삽입 */
static void insert_to_free_lst(void *bp) 
{

  size_t bp_size = GET_SIZE(HDRP(bp));
  int idx = find_idx(bp_size);


  if (free_array[idx] != NULL) {  // 첫 insert가 아닐때
    PRED(bp) = NULL;
    SUCC(bp) = free_array[idx];
    PRED(free_array[idx]) = bp;
    free_array[idx] = bp;
  }
  else {      // 첫 insert
    PRED(bp) = NULL;
    SUCC(bp) = NULL;
    free_array[idx] = bp;
  }
}

/*블럭 사이즈로 free_array 배열에 들어갈 idx를 반환하는 함수*/
static int find_idx(size_t asize)
{ 
  if (asize < (1 << 5)){
    return 0;
    }
  else if (asize < (1 << 6)){
    return 1;
    }
  else if (asize < (1 << 7)){
    return 2;
    }
  else if (asize < (1 << 8)){
    return 3;
    }  
  else if (asize < (1 << 9)){
    return 4;
    }  
  else if (asize < (1 << 10)){
    return 5;  
    }
  else if (asize < (1 << 11)){
    return 6;
    }
  else {
    return 7;  
    }

}
