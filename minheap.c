#include "minheap.h"

HuffmanNode* HuffmanNode_create(
    unsigned char byte,
    unsigned long long freq,
    HuffmanNode* left,
    HuffmanNode* right
) {
    HuffmanNode* node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    if (node == NULL) return NULL;
    node->byte = byte;
    node->freq = freq;
    node->left = left;
    node->right = right;
    return node;
}

MinHeap* MinHeap_create() {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));
    if (heap == NULL) return NULL;
    heap->size = 0;
    return heap;
}

// Начало === Поддержание свойств кучи ===
static void swap(HuffmanNode** a, HuffmanNode** b) {
    HuffmanNode* temp = *a;
    *a = *b;
    *b = temp;
}

static unsigned char parent(unsigned char i) {
    return (i - 1) / 2;
}

static unsigned char child_left(unsigned char i) {
    return i * 2 + 1;
}

static unsigned char child_right(unsigned char i) {
    return i * 2 + 2;
}

static void heapify_up(MinHeap* heap, unsigned char i) {
    HuffmanNode** nodes = heap->nodes;

    while (1) {
        if (i==0) return;
    
        if (nodes[parent(i)]->freq > nodes[i]->freq) {
            swap(&nodes[parent(i)], &nodes[i]);
            i = parent(i);
        } else return;
    }
}

static void heapify_down(MinHeap* heap, unsigned char i) {
    HuffmanNode** nodes = heap->nodes;

    while (1) {
        if (child_left(i)<=i || child_right(i)<=i ||
            child_left(i)>=heap->size || child_right(i)>=heap->size
        ) return;

        if (nodes[child_left(i)]->freq < nodes[i]->freq) {
            swap(&nodes[child_left(i)], &nodes[i]);
            i = child_left(i);
        } else if (nodes[child_right(i)]->freq < nodes[i]->freq) {
            swap(&nodes[child_right(i)], &nodes[i]);
            i = child_right(i);
        } else return;
    }
}
// Конец === Поддержание свойств кучи ===

void MinHeap_insert(MinHeap* heap, HuffmanNode* node) {
    heap->nodes[heap->size] = node;
    heapify_up(heap, heap->size);
    heap->size++;
}

HuffmanNode* MinHeap_extract(MinHeap* heap) {
    HuffmanNode* res = heap->nodes[0];
    swap(&heap->nodes[0], &heap->nodes[--heap->size]);
    heapify_down(heap, 0);
    return res;
}

// Извлечение всех узлов в формате бинарного дерева
HuffmanNode* MinHeap_extract_tree(MinHeap* heap) {
    while (heap->size != 1) {
        HuffmanNode* node1 = MinHeap_extract(heap);
        HuffmanNode* node2 = MinHeap_extract(heap);

        // Создание детерминированности. Среди двух минимальных по частоте узлов, правее = больше частота
        HuffmanNode* merge_node = NULL;
        if (node1->freq >= node2->freq) {
            merge_node = HuffmanNode_create(0, node1->freq+node2->freq, node2, node1);
        } else {
            merge_node = HuffmanNode_create(0, node1->freq+node2->freq, node1, node2);
        }

        MinHeap_insert(heap, merge_node);
    }

    HuffmanNode* tree = MinHeap_extract(heap);
    return tree;
}