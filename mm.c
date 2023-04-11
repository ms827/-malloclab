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
static int find_size(size_t asize);

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

// 초기 가용 리스트 조작을 위한 기본 상수 및 매크로 정의

#define WSIZE  4    
#define DSIZE  8    
#define NUM 8
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

#define PRED(bp)    (*(char **)(bp))            // 앞 블록 bp
#define SUCC(bp)    (*(char **)(bp + WSIZE))    // 뒷 블록 bp


static char *heap_listp;

static char *free_array[NUM];

int mm_init(void)
{
    for (int i=0; i < NUM; i++) {
      free_array[i] = NULL;
    }   
    /*creat'e the initial empty heap*/
    // printf("mm_init()\n");
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
static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;
    // printf("extend_heap()\n");

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
    // printf("mm_free()\n");

  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

static void *coalesce(void *bp)
{
  // printf("coalesce\n");
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc){
  // printf("coalesce case1\n");
    insert_to_free_lst(bp);
    return bp;
  }

  else if (prev_alloc && !next_alloc){
  // printf("coalesce case2\n");
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    remove_free_list(NEXT_BLKP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));

  }

  else if (!prev_alloc && next_alloc){
  // printf("coalesce cas3\n");

    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    remove_free_list(PREV_BLKP(bp));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    bp = PREV_BLKP(bp);   

  }

  else {
  // printf("coalesce cas4\n");
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    remove_free_list(NEXT_BLKP(bp));
    remove_free_list(PREV_BLKP(bp));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));    
    bp = PREV_BLKP(bp);    


  }
  insert_to_free_lst(bp);
  return bp;
}

void *mm_malloc(size_t size)
{
  // printf("mm_malloc\n");
  size_t asize;
  size_t extendsize;
  char *bp;

  if (size ==0)
    return NULL;
  
  if (size <= DSIZE)
    asize = 2*DSIZE;
  else 
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    // printf("size : %d \n\n",asize);
  if ((bp = find_fit(asize)) != NULL){
    place(bp, asize);
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
  // printf("find_fit\n");
  // printf("asize %d\n", asize);

    void* bp;
    
    for (int i = find_size(asize); i < NUM; i++){
      if(free_array[i]==NULL){
        continue;
      }
      for(bp=free_array[i]; bp != NULL; bp=SUCC(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
  }
    return NULL; 
}

static void place(void *bp, size_t asize)
{
  // printf("place\n");

  size_t csize = GET_SIZE(HDRP(bp));
  remove_free_list(bp);

  if ((csize-asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));
    insert_to_free_lst(bp);

  }
  else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));

  }
}



static void remove_free_list(void *bp)
{
  // printf("remove_free_list\n");

  // int idx = find_size(GET_SIZE(HDRP(bp)));
  int idx = find_size(GET_SIZE(HDRP(bp)));
  // printf("idx : %d\n", idx);
  // printf("bp : %x\n", bp);
  // printf("PRED(bp) : %x\n", PRED(bp));
  // printf("SUCC(bp) : %x\n", SUCC(bp));

  if (free_array[idx] != bp){   // 중간블록
    if (SUCC(bp) != NULL) {
      SUCC(PRED(bp)) = SUCC(bp);
      PRED(SUCC(bp)) = PRED(bp);
      }
    else{                       // 맨 뒷 블럭 remove
      // printf("bp : %x\n", bp);
      // printf("SUCC(bp) : %x\n", SUCC(bp));
      SUCC(PRED(bp)) = NULL;
    }
  }
  else{                        // 맨앞 블록
    if (SUCC(bp) != NULL){
      PRED(SUCC(bp)) = free_array[idx];
      free_array[idx] = SUCC(bp);
    } 
    else{                     // 블록이 단 하나
      free_array[idx] = NULL;
    }
  }
}


static void insert_to_free_lst(void *bp) 
{
  // printf("insert_to_free_lst\n");
  size_t bp_size = GET_SIZE(HDRP(bp));
  int idx = find_size(bp_size);

  // printf("idx : %d\n", idx);

  if (free_array[idx] != NULL) {  // 두번 이상 insert
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

static int find_size(size_t asize)
{ 
  // printf("find_size\n");

  if (asize < (1 << 5)){
    return 0;
    }
  else if (asize >= (1 << 5) && asize < (1 << 6)){
    return 1;
    }
  else if (asize >= (1 << 6) && asize < (1 << 7)){
    return 2;
    }
  else if (asize >= (1 << 7) && asize < (1 << 8)){
    return 3;
    }  
  else if (asize >= (1 << 8) && asize < (1 << 9)){
    return 4;
    }  
  else if (asize >= (1 << 9) && asize < (1 << 10)){
    return 5;  
    }
  else if (asize >= (1 << 10) && asize < (1 << 11)){
    return 6;
    }
  else {
    return 7;  
    }

}
