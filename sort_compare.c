#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 10000    /* 데이터 개수 */
#define TRIALS 100 /* 무작위 데이터 실험 반복 횟수 */
#define MAXKEY 1000000
#define SEED 2026         /* 실험 결과를 재현하기 위한 고정 시드 */
#define PARTIAL_PERCENT 5 /* 부분 정렬 데이터의 무작위 교환 횟수를 N에 대한 백분율로 지정 */

typedef struct
{
    int key1;  /* 정렬 기준 키 */
    int key2;  /* 관찰용 키이며 정렬 기준으로 사용하지 않음 */
    int order; /* 원래 입력 순서: 안정성 검사에만 사용하며 정렬 기준에 포함하지 않음 */
} Data;

/* 두 전역 카운터: 각 정렬을 시작하기 전에 초기화 */
static long long cmp_cnt = 0; /* key1 비교 횟수 */
static long long mov_cnt = 0; /* Data 전체의 이동 횟수 */

/* ---------- 보조 함수 ---------- */

/* rand()의 범위가 0~32767인 환경도 고려하여 두 번의 난수를 결합 */
static int rnd(int maxv)
{
    unsigned r = ((unsigned)(rand() & 0x7FFF) << 15) | (unsigned)(rand() & 0x7FFF);
    return (int)(r % (unsigned)(maxv + 1));
}

static void gen_random(Data a[], int n, int maxkey)
{
    for (int i = 0; i < n; i++)
    {
        a[i].key1 = rnd(maxkey);
        a[i].key2 = rnd(MAXKEY);
        a[i].order = i;
    }
}

/* 실험 입력 준비용 비교 함수: 비교 및 이동 횟수 측정에서 제외 */
static int cmp_for_prepare(const void *x, const void *y)
{
    const Data *p = x, *q = y;
    if (p->key1 != q->key1)
        return (p->key1 > q->key1) - (p->key1 < q->key1);
    return (p->order > q->order) - (p->order < q->order);
}

/* 정렬 순서와 함께 원본 레코드의 누락/중복/손상도 확인한다.
   검사에 필요한 비교와 복사는 정렬 횟수에 포함하지 않는다. */
static int check_result(const Data result[], const Data input[], int n)
{
    int seen[N] = {0};
    Data original[N];

    for (int i = 0; i < n; i++)
    {
        int id = input[i].order;
        if (id < 0 || id >= n || seen[id])
            return 0;
        seen[id] = 1;
        original[id] = input[i];
    }
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < n; i++)
    {
        int id = result[i].order;
        if (i > 0 && result[i - 1].key1 > result[i].key1)
            return 0;
        if (id < 0 || id >= n || seen[id])
            return 0;
        seen[id] = 1;
        if (result[i].key1 != original[id].key1 ||
            result[i].key2 != original[id].key2)
            return 0;
    }
    return 1;
}

/* 같은 key1을 가진 인접 데이터 중 원래 순서가 뒤집힌 쌍의 수를 반환. 0이면 이 입력에서 순서 유지 */
static int stability_violations(const Data a[], int n)
{
    int v = 0;
    for (int i = 0; i + 1 < n; i++)
        if (a[i].key1 == a[i + 1].key1 && a[i].order > a[i + 1].order)
            v++;
    return v;
}

/* 임시 변수를 이용한 두 데이터 교환: 총 3회 이동 */
static void swap_data(Data *x, Data *y)
{
    Data t = *x;
    mov_cnt++;
    *x = *y;
    mov_cnt++;
    *y = t;
    mov_cnt++;
}

/* ---------- 정렬 알고리즘 ---------- */

/* [1] 선택 정렬: min != i일 때만 교환 (README에 명시할 측정 기준) */
static void selection_sort(Data a[], int n)
{
    for (int i = 0; i < n - 1; i++)
    {
        int min = i;
        for (int j = i + 1; j < n; j++)
        {
            cmp_cnt++;
            if (a[j].key1 < a[min].key1)
                min = j;
        }
        if (min != i)
            swap_data(&a[i], &a[min]);
    }
}

/* [2] 일반 버블 정렬: 항상 n-1회의 순회를 수행 */
static void bubble_sort(Data a[], int n)
{
    for (int pass = 0; pass < n - 1; pass++)
    {
        for (int j = 0; j < n - 1 - pass; j++)
        {
            cmp_cnt++;
            if (a[j].key1 > a[j + 1].key1)
                swap_data(&a[j], &a[j + 1]);
        }
    }
}

/* [2'] 조기 종료 버블 정렬: 한 순회에서 교환이 없으면 종료 */
static void bubble_sort_early(Data a[], int n)
{
    for (int pass = 0; pass < n - 1; pass++)
    {
        int swapped = 0;
        for (int j = 0; j < n - 1 - pass; j++)
        {
            cmp_cnt++;
            if (a[j].key1 > a[j + 1].key1)
            {
                swap_data(&a[j], &a[j + 1]);
                swapped = 1;
            }
        }
        if (!swapped)
            break;
    }
}

/* [3] 삽입 정렬. strict = 1: 경우 A (>), strict = 0: 경우 B (>=) */
static void insertion_core(Data a[], int n, int strict)
{
    for (int i = 1; i < n; i++)
    {
        Data cur = a[i];
        mov_cnt++; /* 임시 변수에 저장 */
        int j = i - 1;
        while (j >= 0)
        {
            cmp_cnt++;
            int shift = strict ? (a[j].key1 > cur.key1)
                               : (a[j].key1 >= cur.key1);
            if (!shift)
                break;
            a[j + 1] = a[j];
            mov_cnt++; /* 오른쪽으로 한 칸 이동 */
            j--;
        }
        a[j + 1] = cur;
        mov_cnt++; /* 최종 위치에 삽입 */
    }
}
static void insertion_sort_A(Data a[], int n) { insertion_core(a, n, 1); } /* > 조건 사용 */
static void insertion_sort_B(Data a[], int n) { insertion_core(a, n, 0); } /* >= 조건 사용 */

/* ---------- 실험 실행 ---------- */

/* 번호로 정렬 함수를 선택하여 각 실험에서 같은 순서로 실행한다. */
static const char *algorithm_name[5] = {
    "Selection Sort", "Bubble Sort", "Bubble Sort (Early Termination)",
    "Insertion Sort (>)", "Insertion Sort (>=)"};

static int run_one(int algorithm, const Data src[], Data work[], int n,
                   long long *cmp, long long *mov)
{
    memcpy(work, src, sizeof(Data) * n); /* 실험 전 복사는 측정하지 않음 */
    cmp_cnt = 0;
    mov_cnt = 0;

    switch (algorithm)
    {
    case 0:
        selection_sort(work, n);
        break;
    case 1:
        bubble_sort(work, n);
        break;
    case 2:
        bubble_sort_early(work, n);
        break;
    case 3:
        insertion_sort_A(work, n);
        break;
    case 4:
        insertion_sort_B(work, n);
        break;
    default:
        return 0;
    }
    *cmp = cmp_cnt;
    *mov = mov_cnt;
    return check_result(work, src, n);
}

static void report_single(const char *title, const Data src[], Data work[])
{
    printf("%s\n\n", title);
    for (int k = 0; k < 4; k++)
    {
        long long c, m;
        int ok = run_one(k, src, work, N, &c, &m);
        printf("%s\n", algorithm_name[k]);
        printf("Comparisons : %lld\n", c);
        printf("Movements   : %lld\n", m);
        printf("Sorted + data check: %s\n\n", ok ? "OK" : "FAIL");
    }
    printf("------------------------------------\n\n");
}

static void report_stability(const char *name, int algorithm, const Data src[], Data work[], int n)
{
    long long c, m;
    int ok = run_one(algorithm, src, work, n, &c, &m);
    int v = stability_violations(work, n);
    printf("%-21s: %s (sorted=%s, violations=%d)\n",
           name, v == 0 ? "STABLE" : "NOT STABLE", ok ? "OK" : "FAIL", v);
}

/* 선택 정렬에서 동일 키의 순서가 바뀌는 것을 보여 주는 데이터 3개 예제 */
static void small_demo(void)
{
    Data d[3] = {{5, 100, 0}, {5, 200, 1}, {2, 300, 2}};
    printf("Small example (Key1, Key2): (5,100) (5,200) (2,300)\n");
    Data t[3];
    memcpy(t, d, sizeof d);
    selection_sort(t, 3);
    printf("  Selection      -> (%d,%d) (%d,%d) (%d,%d)\n",
           t[0].key1, t[0].key2, t[1].key1, t[1].key2, t[2].key1, t[2].key2);
    memcpy(t, d, sizeof d);
    insertion_sort_A(t, 3);
    printf("  Insertion (>)  -> (%d,%d) (%d,%d) (%d,%d)\n",
           t[0].key1, t[0].key2, t[1].key1, t[1].key2, t[2].key1, t[2].key2);
    memcpy(t, d, sizeof d);
    insertion_sort_B(t, 3);
    printf("  Insertion (>=) -> (%d,%d) (%d,%d) (%d,%d)\n\n",
           t[0].key1, t[0].key2, t[1].key1, t[1].key2, t[2].key1, t[2].key2);
}

int main(void)
{
    static Data base[N], work[N], sorted_d[N], reverse_d[N], partial_d[N];
    srand(SEED);

    printf("Data Count : %d\nSeed : %d\n\n", N, SEED);

    /* ===== 1. 무작위 데이터: 100회 반복 ===== */
    long long tc[4] = {0}, tm[4] = {0};
    int all_ok = 1;
    for (int t = 0; t < TRIALS; t++)
    {
        gen_random(base, N, MAXKEY); /* 네 알고리즘이 함께 사용할 새로운 입력 데이터 생성 */
        for (int k = 0; k < 4; k++)
        {
            long long c, m;
            if (!run_one(k, base, work, N, &c, &m))
                all_ok = 0;
            tc[k] += c;
            tm[k] += m;
        }
        if ((t + 1) % 10 == 0)
        {
            fprintf(stderr, "Random trials: %d / %d completed\n", t + 1, TRIALS);
        }
    }
    printf("Random Data\nNumber of Trials : %d\n\n", TRIALS);
    for (int k = 0; k < 4; k++)
    {
        printf("%s\n", algorithm_name[k]);
        printf("Total Comparisons   : %lld\n", tc[k]);
        printf("Average Comparisons : %.2f\n", (double)tc[k] / TRIALS);
        printf("Total Movements     : %lld\n", tm[k]);
        printf("Average Movements   : %.2f\n\n", (double)tm[k] / TRIALS);
    }
    printf("All %d trials sorted and data preserved: %s\n", TRIALS, all_ok ? "YES" : "NO");
    printf("\n------------------------------------\n\n");

    /* ===== 2. 동일한 무작위 원본으로 오름차순 / 역순 / 부분 정렬 입력 준비 ===== */
    gen_random(base, N, MAXKEY);
    memcpy(sorted_d, base, sizeof base);
    qsort(sorted_d, N, sizeof(Data), cmp_for_prepare); /* 실험 입력 준비에만 사용 */
    for (int i = 0; i < N; i++)
        reverse_d[i] = sorted_d[N - 1 - i];
    memcpy(partial_d, sorted_d, sizeof sorted_d);
    int swaps = N * PARTIAL_PERCENT / 100; /* N의 5%에 해당하는 500회 교환: 실제로 바뀐 위치의 비율과는 다름 */
    for (int s = 0; s < swaps; s++)
    {
        int i = rnd(N - 1), j = rnd(N - 1);
        Data t = partial_d[i];
        partial_d[i] = partial_d[j];
        partial_d[j] = t;
    }

    report_single("Sorted Data", sorted_d, work);
    report_single("Reverse-Sorted Data", reverse_d, work);
    printf("(Partially sorted: sorted array + %d random swaps = %d%% of N)\n\n", swaps, PARTIAL_PERCENT);
    report_single("Partially Sorted Data", partial_d, work);

    /* ===== 3. 안정성 검증 ===== */
    printf("Stability Verification\n\n");
    small_demo();

    static Data dup[N];
    gen_random(dup, N, 50); /* key1의 범위를 0~50으로 제한하여 중복 키가 많은 입력 생성 */
    printf("Test data: %d items, Key1 in [0,50] (about %d items per key value)\n\n", N, N / 51);
    report_stability("Selection Sort", 0, dup, work, N);
    report_stability("Bubble Sort", 1, dup, work, N);
    report_stability("Bubble Sort (Early)", 2, dup, work, N);
    report_stability("Insertion Sort (>)", 3, dup, work, N);
    report_stability("Insertion Sort (>=)", 4, dup, work, N);

    /* 중복 키 데이터에서 >와 >= 조건에 따른 이동 횟수 비교 */
    long long c, m;
    run_one(3, dup, work, N, &c, &m);
    printf("\nInsertion (>)  on dup data: Comparisons=%lld, Movements=%lld\n", c, m);
    run_one(4, dup, work, N, &c, &m);
    printf("Insertion (>=) on dup data: Comparisons=%lld, Movements=%lld\n", c, m);
    return 0;
}
