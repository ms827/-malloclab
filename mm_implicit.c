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
// size보다 큰 가장 가까운 ALIGNMENT의 배수로 만들어준다 -> 정렬!
// '& ~0x7' 작업은 'ALIGN'의 결과가 항상 8의 배수가 되도록 하는 데 사용되며, 이는 대부분의 최신 프로세서의 정렬 요구 사항을 충족
// size = 7 : (00000111 + 00000111) & 11111000 = 00001110 & 11111000 = 00001000 = 8
// size = 13 : (00001101 + 00000111) & 11111000 = 00010000 = 16
// 1 ~ 7 bytes : 8 bytes
// 8 ~ 16 bytes : 16 bytes
// 17 ~ 24 bytes : 24 bytes
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) //size_t 값의 크기를 바이트 단위로 계산하여 정렬 값의 가장 가까운 배수로 반올림합니다.

/* 기본 단위인 word, double word, 새로 할당받는 힙의 크기 CHUNKSIZE를 정의한다 */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount : 4096bytes -> 4kib */

#define MAX(x, y) ((x) > (y) ? (x) : (y))  // 최댓값 구하는 함수 매크로

/* header 및 footer 값(size + allocated) 리턴 */
// 더블워드 정렬로 인해 size의 오른쪽 3~4자리는 비어 있다.
// 이 곳에 0(freed), 1(allocated) flag를 삽입한다.
#define PACK(size, alloc)   ((size) | (alloc))

/* 주소 p에서의 word를 읽어오거나 쓰는 함수 */
// 포인터 p가 가리키는 곳의 값을 리턴하거나 val을 저장
#define GET(p)          (*(unsigned int*)(p))
#define PUT(p, val)     (*(unsigned int*)(p) = (val))

/* header or footer에서 블록의 size, allocated field를 읽어온다 */
// & ~0x7 => 0x7:0000 0111 ~0x7:1111 1000이므로 ex. 1011 0111 & 1111 1000 = 1011 0000 : size 176bytes
// & 0x1 => ex. 1011 0111 | 0000 0001 = 1 : Allocated!
#define GET_SIZE(p)     (GET(p) & ~0x7)     
#define GET_ALLOC(p)    (GET(p) & 0x1)    

/* 블록 포인터 bp를 인자로 받아 블록의 header와 footer의 주소를 반환한다 */
// 포인터가 char* 형이므로, 숫자를 더하거나 빼면 그 만큼의 바이트를 뺀 것과 같다.
// WSIZE 4를 뺀다는 것은 주소가 4byte(1 word) 뒤로 간다는 뜻. bp의 1word 뒤는 헤더.

/* HDRP
힙에 있는 블록의 헤더는 블록 자체 바로 앞에 저장됩니다. 
따라서 블록의 헤더에 대한 포인터를 얻으려면 블록 포인터 bp에서 헤더의 크기(이 코드에서는 WSIZE)를 빼야 합니다.

(char*) 캐스트는 블록 포인터 bp를 바이트 포인터인 char* 유형으로 변환하는 데 사용됩니다. 
이를 통해 바이트 수준 세분성으로 포인터 산술을 수행할 수 있습니다. 
bp에서 WSIZE를 빼면 블록 헤더의 시작에 대한 포인터를 얻습니다.
*/
/* FTRP
힙에 있는 블록의 바닥글은 블록 자체의 끝 직후에 저장됩니다. 
따라서 블록의 바닥글에 대한 포인터를 얻으려면 블록 포인터 'bp'에 블록 크기(블록 헤더에 저장됨)를 더한 다음 바닥글 크기(블록 헤더에 저장됨)를 빼야 합니다. 이 코드의 DSIZE).

(char*) 캐스트는 블록 포인터 bp를 바이트 포인터인 char* 유형으로 변환하는 데 사용됩니다. 
이를 통해 바이트 수준 세분성으로 포인터 산술을 수행할 수 있습니다. bp에 GET_SIZE(HDRP(bp))를 추가하여 블록의 끝(즉, 페이로드 바로 뒤의 주소)에 대한 포인터를 얻습니다.

마지막 단계는 블록 바닥글의 시작에 대한 포인터를 얻기 위해 결과에서 'DSIZE'를 빼는 것입니다. 
이는 바닥글이 블록의 끝 바로 앞에 오는 더블 워드(8바이트)이기 때문입니다.
*/
#define HDRP(bp)    ((char*)(bp) - WSIZE) 
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 블록 포인터 bp를 인자로 받아 이후, 이전 블록의 주소를 리턴한다 */
/* 
NEXT_BLKP : 지금 블록의 bp에 블록의 크기(char*이므로 word단위)만큼을 더한다.
힙에서 다음 블록의 주소는 현재 블록 포인터 'bp'에 현재 블록의 크기를 더하여 계산할 수 있습니다. 
현재 블록의 크기는 메모리에서 현재 블록 바로 앞에 있는 현재 블록의 헤더에 저장됩니다. 
따라서 헤더에 대한 포인터를 얻으려면 현재 블록 포인터 bp에서 헤더 크기(이 코드에서는 WSIZE)를 빼야 합니다.
그러나 현재 블록의 헤더 자체는 블록 크기에 포함되지 않습니다. 따라서 다음 블록의 시작에 대한 포인터를 얻으려면 WSIZE를 다시 추가해야 합니다.

(char*) 캐스트는 블록 포인터 bp를 바이트 포인터인 char* 유형으로 변환하는 데 사용됩니다. 이를 통해 바이트 수준 세분성으로 포인터 산술을 수행할 수 있습니다. 
bp에서 WSIZE를 빼면 블록 헤더에 대한 포인터를 얻습니다.

마지막 단계는 현재 블록의 크기(블록 헤더에서 'GET_SIZE'를 호출하여 얻음)를 이 포인터에 추가하여 다음 블록의 시작에 대한 포인터를 얻는 것입니다.
*/

/* 
PREV_BLKP : 지금 블록의 bp에 이전 블록의 footer에서 참조한 이전 블록의 크기를 뺀다.
이전 블록의 주소를 계산하려면 메모리에서 현재 블록 바로 앞에 있는 이전 블록의 푸터에 액세스해야 합니다. 
바닥글에는 이전 블록의 크기가 포함되어 있으므로 이전 블록의 시작에 대한 포인터를 얻으려면 현재 블록 포인터 'bp'에서 이 크기를 빼야 합니다.

그러나 이전 블록의 바닥글 자체는 이전 블록의 크기에 포함되지 않습니다. 
따라서 바닥글에 대한 포인터를 얻으려면 현재 블록 포인터 bp에서 DSIZE를 빼야 합니다.

(char*) 캐스트는 블록 포인터 bp를 바이트 포인터인 char* 유형으로 변환하는 데 사용됩니다. 
이를 통해 바이트 수준 세분성으로 포인터 산술을 수행할 수 있습니다. bp에서 DSIZE를 빼면 블록 바닥글에 대한 포인터를 얻습니다.

마지막 단계는 이전 블록의 시작에 대한 포인터를 얻기 위해 이 포인터에서 이전 블록의 크기(블록 바닥글에서 GET_SIZE를 호출하여 얻음)를 빼는 것입니다.
*/

#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // (char*)(bp) + GET_SIZE(지금 블록의 헤더값)
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // (char*)(bp) - GET_SIZE(이전 블록의 풋터값)

/* global variable & functions */
static char *heap_listp; // 항상 prologue block을 가리키는 정적 전역 변수 설정

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 메모리에서 4word 가져오고 이걸로 빈 가용 리스트 초기화 */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) 
        return -1;

    PUT(heap_listp, 0);   
    PUT(heap_listp, PACK(DSIZE, 1));               /* Alignment padding . 더블 워드 경계로 정렬된 미사용 패딩. */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header ** when find func, note endpoint */
    heap_listp += (2 * WSIZE);                     /* 정적 전역 변수는 늘 prologue block을 가리킨다.*/

    /* 그 후 CHUNKSIZE만큼 힙을 확장해 초기 가용 블록을 생성한다. */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * extend_heap
 이 함수는 2가지 경우에 호출될 수 있다.
 1.힙이 초기화될 때
   초기화 후에 초기 가용 블록을 생성하기 위해 호출된다.
 2.요청한 크기의 메모리 할당을 위해 충분한 공간을 찾지 못했을 때
   Double word 정렬을 위해 8의 배수로 반올림하고, 추가 힙 공간을 요청한다.
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    /* 더블 워드 정렬에 따라 메모리를 mem_sbrk 함수를 이용해 할당받는다. */
    // Double Word Alignment : 늘 짝수 개수의 워드를 할당해주어야 한다.
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;   // size를 짝수 word && byte 형태로 만든다.
    if ((long)(bp = mem_sbrk(size)) == -1)                      // 새 메모리의 첫 부분을 bp로 둔다. 주소값은 int로는 못 받아서 long으로 casting.
        return NULL;
    
    /* 새 가용 블록의 header와 footer를 정해주고 epilogue block을 가용 블록 맨 끝으로 옮긴다. */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header 할당 안 해줬으므로 0으로.*/
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}




/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // 해당 블록의 size를 알아내 header와 footer의 정보를 수정한다.
    size_t size = GET_SIZE(HDRP(bp));

    // header와 footer를 설정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 만약 앞뒤의 블록이 가용 상태라면 연결한다.
    coalesce(bp);
}
/*
    Case 1 : 이전과 다음 블록이 모두 할당되어 있다.
        - 현재 블록만 가용 상태로 변경한다.

    Case 2 : 이전 블록은 할당 상태, 다음 블록은 가용 상태이다.
        - 현재 블록과 다음 블록을 통합한다.

    Case 3 : 이전 블록은 가용 상태, 다음 블록은 할당 상태이다.
        - 이전 블록과 현재 블록을 통합한다.

    Case 4 : 이전 블록과 다음 블록 모두 가용 상태이다.
        - 이전 블록, 현재 블록, 다음 블록을 통합한다.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {                /* case1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {          /* case2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {          /* case3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                         /* case4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

void *mm_malloc(size_t size) 
{
    size_t asize;       // adjusted block szie
    size_t extendsize;  // Amount to extend heap if no fit
    char *bp;

    // Ignore spurious requests
    if (size == 0) 
        return NULL;
    
    // Adjust block size to include overhead and alignment reqs
    if (size <= DSIZE)     // 2words 이하의 사이즈는 4워드로 할당 요청 (header 1word, footer 1word)
        asize = 2*DSIZE;
    
    else                  // 할당 요청의 용량이 2words 초과 시, 충분한 8byte의 배수의 용량 할당
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    

    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {   // 적당한 크기의 가용 블록 검색
        place(bp, asize);                   // 초과 부분을 분할하고 새롭게 할당한 블록의 포인터 반환
        return bp;
    } 

    // NO fit found. Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)     // 칸의 개수
        return NULL;
    place(bp, asize);
    return bp;
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;     // 크기를 조절하고 싶은 힙의 시작 포인터
    void *newptr;           // 크기 조절 뒤의 새 힙의 시작 포인터
    size_t copySize;        // 복사할 힙의 크기

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));

    // 원래 메모리 크기보다 적은 크기를 realloc하면 
    // 크기에 맞는 메모리만 할당되고 나머지는 안 된다. 
    if (size < copySize)
      copySize = size;
    // mempy(복사되는 공간의 첫번째 시작주소, 복사할 메모리의 첫번째 시작주소, 복사할 크기)
    memcpy(newptr, oldptr, copySize);   // newptr에 oldptr를 시작으로 copySize만큼의 메모리 값을 복사한다.
    mm_free(oldptr);                    // 기존의 힙을 반환한다.
    return newptr;
}


static void *find_fit(size_t asize) 
{
    void *bp;
    // 프롤로그 블록에서 에필로그 블록 전까지 블록 포인터 bp를 탐색한다.
    // 블록이 가용 상태이고 사이즈가 요구 사이즈보다 크다면 해당 블록 포인터를 리턴
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;    // NO fit
}


static void place(void *bp, size_t asize) 
{
    size_t csize = GET_SIZE(HDRP(bp));  // 현재 할당할 수 있는 후보 가용 블록의 주소

    // 분할이 가능한 경우
    // -> 남은 메모리가 최소한의 가용 블록을 만들 수 있는 4word(16byte)가 되느냐.
    // header & footer : 1word씩, payload : 1word, 정렬 위한 padding : 1word = 4words
    if ((csize - asize) >= (2 * DSIZE)) {
        // 앞의 블록은 할당 블록으로
        // 요청 용량 만큼 블록 배치
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        bp = NEXT_BLKP(bp);
        // 뒤의 블록은 가용 블록으로 분할한다.
        // 남은 블록에 header, footer 배치
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {      // csize와 aszie 차이가 네 칸(16byte)보다 작다면 해당 블록 통째로 사용 남은 부분은 padding한다.
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}



