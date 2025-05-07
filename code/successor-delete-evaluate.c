// ================================================================
//
//   Source code for the experimental results contained in the paper:
//
//     "A Simple Integer Successor-Delete Data Structure", 
//     by Gerth Stølting Brodal.
//     23rd Symposium on Experimental Algorithm (SEA 2025)
//
//   Evaluates the running time of various data structures supporting 
//   successor and delete operations on an initial set of integers 
//   {0, ..., n+1}, where the entries 0 and n+1, should not be deleted.
//
// ================================================================

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

typedef long long int value;  // 64 bit values for set elements

const value WORD_SIZE = 8 * sizeof(value);  // number of bits in value
const value MIN_N = 2;                      // min input size
const value MAX_N = 1 << 22;                // max set size n
const value MAX_OPERATIONS = 9 * MAX_N + 1; // max operations in test
const double MIN_TEST_TIME = 1.0;           // min test time
const value MIN_REPEATS = 5;                // min number of repeats in a timing
const value BEST_OF = 3;                    // timing repeats
const char *DATAFILE = "../data/data.csv";  // output file

typedef struct {
    char *name;
    void (*init)(value);
    void (*delete)(value);
    value (*successor)(value);
} Algorithm;

// ================================================================
//   Successor-delete data structure from pseudocodes in the paper
// ================================================================

value *A;  // global successor-delete array

void allocate(value max_n) {
    A = malloc((max_n + 2) * sizeof(value));
}

void init(value n) {
    for (value i = 0; i < n + 2; i++) {
        A[i] = i;
    }
}

void delete(value i) {
    A[i] = i + 1;
}

void delete_checked(value i) {
    if (A[i] == i) {
        A[i] = i + 1;
    }
}

value successor_naive(value i) {
    // no path compression
    while (i < A[i]) { 
        i = A[i];
    }
    return i;
}

value successor_recursive(value i) {
    // recursive path compression
    if (i < A[i]) {
        A[i] = successor_recursive(A[i]);
    }
    return A[i];
}

value successor_2pass(value i) {
    // 2-pass path compression
    value r = i;
    while (r < A[r]) {
        r = A[r];
    }
    while (A[i] < r) {
        value i_next = A[i];
        A[i] = r;
        i = i_next;
    }
    return r;
}

value successor_halving(value i) {
    // path halving (1-pass)
    while (i < A[i]) {
        i = A[i] = A[A[i]];
    }
    return i;
}

const Algorithm alg_naive = {"successor, no compression", init, delete, successor_naive};
const Algorithm alg_recursive = {"successor, recursive", init, delete, successor_recursive};
const Algorithm alg_2pass = {"successor, 2-pass", init, delete, successor_2pass};
const Algorithm alg_2pass_checked = {"successor, 2-pass, checked", init, delete_checked, successor_2pass};
const Algorithm alg_halving = {"successor, halving", init, delete, successor_halving};

// ================================================================
//  Weighted quick-find union-find data structure (McIlroy and Morris)
// ================================================================

typedef struct { value root, weight, succ; } QF_node;

QF_node *QF;  // global quick-find data structure

void QF_allocate(value max_n) {
    QF = malloc((max_n + 2) * sizeof(QF_node));
}

void QF_init(value n) {
    for (value i = 0; i < n + 2; i++) {
        QF[i].root = i;
        QF[i].weight = 1;
        QF[i].succ = i;
    }
}

value QF_successor(value i) {
    return QF[QF[i].root].succ;
}

void QF_delete(value i) {
    if (QF[QF[i].root].succ == i) {  // i == succ(i), i.e., i is not deleted
        value r1 = QF[i].root, r2 = QF[i+1].root;
        if (QF[r1].weight <= QF[r2].weight) {
            QF[r2].weight += QF[r1].weight;
            for (value r=i; QF[r].root == r1; r--) QF[r].root = r2;
        } else {
            QF[r1].succ = QF[r2].succ;
            QF[r1].weight += QF[r2].weight;
            for (value r=i+1; QF[r].root == r2; r++) QF[r].root = r1;
        }
    }
}   

const Algorithm alg_quick_find = {"quick find", QF_init, QF_delete, QF_successor};

// ================================================================
//  Union-find data structure with union by weight and 2-pass path compression
// ================================================================

typedef struct { value parent, weight, succ; } UF_node;

UF_node *UF;  // global union-find data structure

void UF_allocate(value max_n) {
    UF = malloc((max_n + 2) * sizeof(UF_node));
}

void UF_init(value n) {
    for (value i = 0; i < n + 2; i++) {
        UF[i].parent = i;
        UF[i].weight = 1;
        UF[i].succ = i;
    }
}

value UF_find(value i) {
    value r = i;
    while (UF[r].parent != r) {
        r = UF[r].parent;
    }
    while (i != r) {
        value p = UF[i].parent;
        UF[i].parent = r;
        i = p;
    }   
    return r;
}

void UF_union(value i, value j) {
    value r1 = UF_find(i);
    value r2 = UF_find(j);
    if (r1 == r2) {
        return;
    }
    if (UF[r1].weight <= UF[r2].weight) {
        UF[r2].weight += UF[r1].weight;
        UF[r1].parent = r2;
    } else {
        UF[r1].weight += UF[r2].weight;
        UF[r2].parent = r1;
        UF[r1].succ = UF[r2].succ;
    }    
}

value UF_successor(value i) {
    return UF[UF_find(i)].succ;
}

void UF_delete(value i) {
    UF_union(i, i + 1);
}   

const Algorithm alg_union_find = {"union find", UF_init, UF_delete, UF_successor};

// ================================================================
//  Generic successor-delete structure with mircosets
// ================================================================

value *microsets;               // global array of microsets
const Algorithm *alg_macroset;  // algorithm to use for macroset structure

void microset_allocate(value max_n) {
    value n_buckets = (max_n + 2 + WORD_SIZE - 1) / WORD_SIZE;
    microsets = malloc(n_buckets * sizeof(value));  
}

void microset_init(value n) {
    value n_buckets = (n + 2 + WORD_SIZE - 1) / WORD_SIZE;
    alg_macroset->init(n_buckets);
    for (value i = 0; i < n_buckets; i++) {
        microsets[i] = (value) -1;  // all ones
    }
}

void microset_delete(value i) {
    value bucket = i / WORD_SIZE;
    value bit = i % WORD_SIZE;
    value mask = (value) 1 << bit;
    if (microsets[bucket] & mask) {
        microsets[bucket] ^= mask;
        if (microsets[bucket] == 0) {
            alg_macroset->delete(bucket);
        }
    }
}

value microset_successor(value i) {
    value bucket = i / WORD_SIZE;
    value bit = i % WORD_SIZE;
    value mask = (value) 1 << bit;

    value W = microsets[bucket];
    value high_bits = W & ~(((value)1 << bit) - 1);
    if (high_bits) {
        return bucket * WORD_SIZE + __builtin_ctzll(high_bits);
    } else {
        value succ_bucket = alg_macroset->successor(bucket + 1);
        return succ_bucket * WORD_SIZE + __builtin_ctzll(microsets[succ_bucket]);
    }
}

void QF_microset_init(value n) {
    alg_macroset = &alg_quick_find;
    microset_init(n);
}

void UF_microset_init(value n) {
    alg_macroset = &alg_union_find;
    microset_init(n);
}

void DS_microset_init(value n) {
    alg_macroset = &alg_2pass;
    microset_init(n);
}

const Algorithm alg_qf_microset = {"quick find, microset", QF_microset_init, microset_delete, microset_successor};
const Algorithm alg_uf_microset = {"union find, microset", UF_microset_init, microset_delete, microset_successor};
const Algorithm alg_ds_microset = {"successor, 2-pass, microset", DS_microset_init, microset_delete, microset_successor};

// ================================================================
//   Successor-delete data structure from pseudocode in the paper 
//   modified to maintain reverse pointers and heights of subtrees;
//   T_deepest_node() returns a node with maximum depth
// ================================================================

typedef struct {
    value parent;       // parent node, i,e, A[i] >= i
    value height;       // height of subtree rooted at this node
    value next, prev;   // double-linked list of all nodes with equal height
    value left, right;  // double-linked list of siblings
    value child;        // left-most child
} successor_delete_node;

successor_delete_node *nodes;  // global successor-delete array
value *roots;           // roots[height] = a node with height h
value max_height;       // max height of all trees

void T_allocate(value max_n) {
    nodes = malloc((max_n + 2) * sizeof(successor_delete_node));
    roots = malloc((max_n + 2) * sizeof(value));
}

void T_init(value n) {
    // Initialize the successor-delete structure to n + 2 singleton trees
    for (value i = 0; i < n + 2; i++) {
        nodes[i].parent = i;
        nodes[i].height = 0;
        nodes[i].left = i;
        nodes[i].right = i;
        nodes[i].child = -1;
        nodes[i].next = i + 1;
        nodes[i].prev = i - 1;
        roots[i] = -1;
    }
    nodes[0].prev = n + 1;
    nodes[n + 1].next = 0;
    max_height = 0;
    roots[0] = 0;
}

value T_height(value i) {
    // Compute the height of the tree rooted at i, assuming children heights are known
    value child = nodes[i].child;
    if (child == -1) {
        return 0;
    }
    value c = child;
    value ch = nodes[c].height;
    while (nodes[c].right != child) {
        c = nodes[c].right;
        if (nodes[c].height > ch) {
            ch = nodes[c].height;
        }
    }
    return 1 + ch;
}

void T_fix_height(value i) {
    // Recompute height of node i, and update the lists nodes of equal height
    value h = nodes[i].height;
    value next = nodes[i].next;
    value prev = nodes[i].prev;
    if (roots[h] == i) {
        roots[h] = next != i ? next : -1;
    }
    if (next != i) {
        nodes[next].prev = prev;
        nodes[prev].next = next;
        nodes[i].next = i;
        nodes[i].prev = i;
    }
    h = T_height(i);
    nodes[i].height = h;
    if (roots[h] != -1) {
        next = roots[h];
        prev = nodes[next].prev;
        nodes[i].next = next;   
        nodes[i].prev = prev;
        nodes[next].prev = i;
        nodes[prev].next = i;
    }
    roots[h] = i;
}

void T_link(value i, value j) {
    // Make i leftmost child of j
    assert(nodes[i].parent == i);
    value right = nodes[j].child;
    nodes[j].child = i;
    nodes[i].parent = j;
    if (right >= 0) {
        value left = nodes[right].left;
        nodes[i].right = right;
        nodes[i].left = left;
        nodes[right].left = i;
        nodes[left].right = i;
    }
}

void T_unlink(value i) {
    // Remove i from the child list of its parent
    value j = nodes[i].parent;
    assert(j > i);
    value left = nodes[i].left;
    value right = nodes[i].right;
    if (nodes[j].child == i) {
        nodes[j].child = right != i ? right : -1;
    }
    nodes[left].right = right;
    nodes[right].left = left;
    nodes[i].parent = i;
    nodes[i].left = i;
    nodes[i].right = i;
}

void T_delete(value i) {
    value j = nodes[i].parent;
    if (j > i) {
        // Unlink i and fix the height of all previous ancestors of i
        T_unlink(i);
        T_fix_height(j);
        while (nodes[j].parent != j) {
            j = nodes[j].parent;
            T_fix_height(j);
        }
    }
    j = i + 1;
    T_link(i, j);
    // Fix the height of all new ancestors of i
    T_fix_height(j);
    while (nodes[j].parent != j) {
        j = nodes[j].parent;
        T_fix_height(j);
    }
    // Adjust max_height, if it has increased
    if (nodes[j].height > max_height) {
        max_height = nodes[j].height;
    }
}

value T_successor(value i) {
    // 2-pass path compression
    value root = i;
    // Find root
    while (root < nodes[root].parent) {
        root = nodes[root].parent;
    }
    // Path compression
    while (i < root) {
        value parent = nodes[i].parent;
        T_unlink(i);
        T_link(i, root);
        T_fix_height(i);
        i = parent;
    }
    T_fix_height(root);
    // Adjust max_height, if it has decreased
    while (roots[max_height] == -1) {
        max_height--;
    }
    return root;
}    

value T_deepest_leaf(value i) {
    // Find a deepest node in tree rooted at i
    value h = nodes[i].height;
    while (h > 0) {
        h--;
        i = nodes[i].child; 
        while (nodes[i].height != h) {
            i = nodes[i].right;
        }    
    }    
    return i;
}    

value T_deepest_node() {
    // Return a node with maximum depth
    return T_deepest_leaf(roots[max_height]);
}    

void T_validate(value n) {
    // Validate integrity of all nodes
    value uncounted_children = 0;
    for (value i=0; i < n + 2; i++) {
        value parent = nodes[i].parent, 
              child = nodes[i].child, 
              next = nodes[i].next, 
              prev = nodes[i].prev, 
              left = nodes[i].left, 
              right = nodes[i].right, 
              height = nodes[i].height;
        assert(i <= parent && parent < n + 2);
        if (parent != i) {
            uncounted_children++;   
        }
        assert(height >= 0);
        if (height == 0) {
            assert(child == - 1);
        } else {
            assert(0 <= child && child < i);
            value c = child;
            value ch = nodes[c].height;
            assert(nodes[c].parent == i);
            uncounted_children--;
            while (nodes[c].right != child) {
                c = nodes[c].right;
                assert(nodes[c].parent == i);
                uncounted_children--;
                if (nodes[c].height > ch) {
                    ch = nodes[c].height;
                }
            }
            assert(height == ch + 1);
        }
        assert(0 <= next && next < n + 2);
        assert(0 <= prev && prev < n + 2);
        assert(nodes[next].prev == i);
        assert(nodes[prev].next == i);
        assert(height == nodes[next].height);
        assert(height == nodes[prev].height);
        
        assert(0 <= left && left < n + 2);
        assert(0 <= right && right < n + 2);
        assert(nodes[right].left == i);
        assert(nodes[left].right == i);
        assert(nodes[right].parent == parent);
        assert(nodes[left].parent == parent);
    }
    value nodes_found = 0;
    for (value h = 0; h <= max_height; h++) {
        value root = roots[h];
        assert(0 <= roots && root < n + 2);
        assert(nodes[root].height == h);
        nodes_found++;
        while (nodes[root].next != roots[h]) {
            root = nodes[root].next;
            assert(nodes[root].height == h);
            nodes_found++;
        }
    }
    assert(uncounted_children == 0);
    assert(nodes_found == n + 2);
}

// ================================================================
//  Struct to store test data = sequence of operations
// ================================================================

struct {
    value n;        // initial set size {1,...,n}
    char name[100]; // text buffer for name of the current test data
    value *input;   // successor(x) for x >= 1, delete(-x) for x <= - 1, 0 = end
    value *output;  // answers to all input operations, 0 for delete
} data;

void data_allocate(value max_m) {
    data.input = malloc((max_m + 1) * sizeof(value));
    data.output = malloc((max_m + 1) * sizeof(value));
}

void data_set_output(const Algorithm *alg) {
    // Set data.output using data.input and algorithm alg
    value n = data.n;
    alg->init(n);
    value *out = data.output;
    for (value *p = data.input; *p != 0; p++) {
        value x = *p;
        if (x > 0) {
            *(out++) = alg->successor(x);
        } else {
            *(out++) = 0;
            alg->delete(-x);
        }
    }
}

void validate(const Algorithm *alg) {
    // Check if successor-delete algorithm alg generates correct output    
    alg->init(data.n);
    for (value *in=data.input, *out=data.output; *in != 0; in++, out++) {
        value x = *in;
        if (x >= 0) {
            assert(*out == alg->successor(x));
        } else {
            assert(*out == 0);
            assert(1 <= -x <= data.n);
            alg->delete(-x);
        }
    }
}

// ================================================================
//   Various test input sequences
// ================================================================

void data_query_one(value n) {
    // Create sequence Delete(1), ..., Delete(n), n x Succ(1)
    printf("Creating Succ(1) input: n = %d\n", n);
    assert(2 * n <= MAX_OPERATIONS);
    data.n = n;
    sprintf(data.name, "query_one");
    value *p = data.input;
    for (value i=1; i <= n; i++, p++) {
        *p = -i;
    }
    for (value i=1; i <= n; i++, p++) {
        *p = 1;
    }
    *p = 0;
    data_set_output(&alg_2pass);
}

void data_worst_case(value n, double queries_per_deletion) {
    // Create sequence Delete(1), ..., Delete(n), interleaved with worst-case queries
    printf("Creating worst-case input: n = %d, alpha = %.3f\n", n, queries_per_deletion);
    assert(1 + n * (1 + queries_per_deletion) <= MAX_OPERATIONS);
    data.n = n;
    sprintf(data.name, "worst_case %.3f", queries_per_deletion);
    value *p = data.input;
    T_init(n);
    value queries = 0;
    for (value i = 1; i <= n; i++) {
        T_delete(i);
        *(p++) = -i;
        while (queries < i * queries_per_deletion) {
            value j = T_deepest_node();
            T_successor(j);
            *(p++) = j;
            queries += 1;
        }
    }
    *p = 0;
    data_set_output(&alg_2pass);
}

unsigned long long random64() {
    // Generate a random 64-bit integer
    value x = 0;
    for (int i = 0; i < 8; i++) {
        x = (x << 8) | (rand() & 255);
    }
    return x;
}

void data_random(value n, double queries_per_deletion) {
    // Create sequence with n random Delete, interleaved with worst-case queries
    printf("Creating random input: n = %d, alpha = %.3f\n", n, queries_per_deletion);
    assert(1 + n * (1 + queries_per_deletion) <= MAX_OPERATIONS);
    data.n = n;
    sprintf(data.name, "random %.3f", queries_per_deletion);
    value *p = data.input;
    T_init(n);
    value queries = 0;
    for (value i = 1; i <= n; i++) {
        value d = random64() % (n - 1) + 1;
        T_delete(d);
        *(p++) = -d;
        while (queries < i * queries_per_deletion) {
            value j = T_deepest_node();
            successor_2pass(j);
            *(p++) = j;
            queries += 1;
        }
    }
    *p = 0;
    data_set_output(&alg_2pass);
}

// ================================================================
//  List of algorithms evaluated
// ================================================================

const int n_algorithms = 10;
const Algorithm algorithms[10] = {
    alg_naive, 
    alg_recursive, 
    alg_2pass, 
    alg_2pass_checked, 
    alg_halving, 
    alg_quick_find, 
    alg_union_find,
    alg_qf_microset,
    alg_uf_microset,
    alg_ds_microset
};

// ================================================================
//  Timing functions
// ================================================================

value trash = 0;  // xor of all successor results, avoid compiler optimization

void time_it(const Algorithm *alg, const char *data_filename) {
    // Time the algorithm alg on the test data in data.input
    validate(alg);  // check if algorithm generates correct output before timing

    void (*init)(value)       = alg->init;
    void (*delete)(value)     = alg->delete;
    value (*successor)(value) = alg->successor;
    value n                   = data.n;
    value *input              = data.input;

    printf("\"%s\", \"%s\", %d, ", alg->name, data.name, n);
    fflush(stdout);
    double seconds = 1e100;
    double best_time = 1e100;
    int r = 0, repeats = MIN_REPEATS;
    for (int repeat_timing = 0; repeat_timing < BEST_OF; repeat_timing++) {
        clock_t start = clock();
        while (1) {
            for (; r < repeats; r++) {
                init(n);
                for (value *in = input; *in != 0; in++) {
                    value x = *in;
                    if (x >= 0) {
                        trash ^= successor(x);
                    } else {
                        delete(-x);
                    }
                }
            }
            clock_t end = clock();
            seconds = ((double) (end - start)) / CLOCKS_PER_SEC;
            if (seconds >= MIN_TEST_TIME) break;
            repeats *= 2;
        }
        seconds /= r;
        if (seconds < best_time) {
            best_time = seconds;
        }
    }
    printf("%.10e\n", best_time);
    FILE *data_file = fopen(data_filename, "a");
    fprintf(data_file, "\"%s\", \"%s\", %d, %.10e\n", alg->name, data.name, n, best_time);
    fclose(data_file);
}

void time_query_one() {
    //  Run tests Delete(1), ..., Delete(n), n x Succ(1)
    for (value n = MIN_N; n <= MAX_N; n *= 2) {
        data_query_one(n);
        for (int s = 0; s < n_algorithms; s++) {
            if (s == 0 && n > 65536) {
                continue;  // naive algorithm too slow
            }
            if (s == 1 && n > 65536) {
                continue;  // recursion limit exceeded for recursive path compression
            }
            time_it(&algorithms[s], DATAFILE);
        }
    }
}

void time_worst_case() {
    // Run tests Delete(1), ..., Delete(n), interleaved with worst-case queries
    for (value n = MIN_N; n <= MAX_N; n *= 2) {
        for (double q = 1.0 / 8; q <= 8; q *= 2) {
            data_worst_case(n, q);
            for (int s = 1; s < n_algorithms; s++) {
                time_it(&algorithms[s], DATAFILE);
            }
        }
    }
}

void time_random() {
    // Run tests with n random Delete, interleaved with worst-case queries
    for (value n = MIN_N; n <= MAX_N; n *= 2) {
        for (double q = 1.0 / 8; q <= 8; q *= 2) {
            data_random(n, q);
            for (int s = 1; s < n_algorithms; s++) {
                time_it(&algorithms[s], DATAFILE);
            }
        }
    }
}

// ================================================================
//   Main
// ================================================================

int main() {
    printf("Values are %d byte integers\n", sizeof(value));
    
    // Allocate space for all global data structures
    allocate(MAX_N);
    T_allocate(MAX_N);
    QF_allocate(MAX_N);
    UF_allocate(MAX_N);
    microset_allocate(MAX_N);
    data_allocate(MAX_OPERATIONS);

    // Run all tests
    time_random();
    time_query_one();
    time_worst_case();

    printf("Trash (ignore): %d\n", trash);
}

// ================================================================
