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
static void append_free_list(void *bp);
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

// #define RE_PRED(bp,rp)    (GET(PRED(bp)) = rp)  // 앞 블록의 주소에 뒤 블록의 주소를 넣어준다
// #define RE_SUCC(bp,rp)    (GET(SUCC(bp)) = rp)  // 뒷 블록의 주소에 앞 블록의 주소를 넣어준다

static char *heap_listp;
static char *free_listp=NULL;


int mm_init(void)
{
    /*create the initial empty heap*/
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
      return -1;
    PUT(heap_listp, 0); /*Alignement padding, heap_listp 가 가르키는 위치에 0 삽입*/
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); /*PUT(주소, 값)Prologue header*/
    PUT(heap_listp + (2*WSIZE), NULL); //PRED(predecessor)
    PUT(heap_listp + (3*WSIZE), NULL); //SUCC(successor)
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1)); /*Prologue footer*/
    PUT(heap_listp + (5*WSIZE), PACK(0,1)); /*Epilogue header*/
    free_listp = heap_listp +(2*WSIZE);


    
    // root = mem_heap_lo();
    //root = heap_listp;
    /*Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
      return -1;
    return 0;
}

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
  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc){
    append_free_list(bp);
    return bp;
  }

  else if (prev_alloc && !next_alloc){
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    remove_free_list(NEXT_BLKP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));

  }

  else if (!prev_alloc && next_alloc){
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    remove_free_list(PREV_BLKP(bp));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    bp = PREV_BLKP(bp);   

  }

  else {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    remove_free_list(NEXT_BLKP(bp));
    remove_free_list(PREV_BLKP(bp));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));    
    bp = PREV_BLKP(bp);    


  }
  append_free_list(bp);
  return bp;
}

void *mm_malloc(size_t size)
{
  size_t asize;
  size_t extendsize;
  char *bp;

  if (size ==0)
    return NULL;
  
  if (size <= DSIZE)
    asize = 2*DSIZE;
  else 
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

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
    void* bp;

    for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = SUCC(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;    // NO fit
}

static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));
  remove_free_list(bp);

  if ((csize-asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));
    append_free_list(bp);

  }
  else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));

  }
}

static void append_free_list(void *bp)
{   
  PRED(bp) = NULL;
  SUCC(bp) = free_listp;
  PRED(free_listp) = bp;
	free_listp = bp;

  // RE_SUCC(bp, free_listp);
  // RE_PRED(free_listp, bp);
  // RE_PRED(bp, NULL);
	// free_listp = bp;
}

static void remove_free_list(void *bp)
{
  if (PRED(bp) != NULL) {   // 중간블록
    SUCC(PRED(bp)) = SUCC(bp);
    PRED(SUCC(bp)) = PRED(bp);
    
  }
  else {                    // 맨앞 블록
    free_listp = SUCC(bp);
    PRED(SUCC(bp)) = PRED(bp);
  }

  // if (PRED(bp) != NULL) {
  //   RE_SUCC(PRED(bp),SUCC(bp));
  //   RE_PRED(SUCC(bp),PRED(bp)); 

  // }
  // else {
  //   free_listp = SUCC(bp);
  //   RE_PRED(SUCC(bp),PRED(bp));
  // }
}

