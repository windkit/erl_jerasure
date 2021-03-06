#include <string.h>
#include <set>

#include "cauchycoding.h"
#include "alloc.h"

#include "jerasure.h"
#include "cauchy.h"

vector<ErlNifBinary> CauchyCoding::doEncode(unsigned char* data, size_t dataSize) {
//vector<BlockEntry> CauchyCoding::doEncode(unsigned char* data, size_t dataSize) {
//    vector<BlockEntry> allBlockEntry;
    vector<ErlNifBinary> allBlockEntry;

    int *matrix = cauchy_good_general_coding_matrix(k, m, w);
    int *bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
    int **smart = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);

    size_t blockSize = roundTo((roundTo(dataSize, k*w) / (k*w)), 4) * w;

    char** dataBlocks = (char**)alloc(sizeof(char*) * k);
    char** codeBlocks = (char**)alloc(sizeof(char*) * m);

    size_t offset = 0;
    for(int i = 0; i < k; ++i) {
        ErlNifBinary tmpBlock;
        enif_alloc_binary(blockSize, &tmpBlock);
        dataBlocks[i] = (char*)tmpBlock.data;
        
        /*
        dataBlocks[i] = (char*)alloc(blockSize);

        BlockEntry tmpBlock;
        tmpBlock.data = (unsigned char*)dataBlocks[i];
        tmpBlock.size = blockSize;
        */

        allBlockEntry.push_back(tmpBlock);

        if (i == k - 1) {
            memset(dataBlocks[i], 0, blockSize);
            memcpy(dataBlocks[i], data + offset, dataSize - offset);
        } else {
            memcpy(dataBlocks[i], data + offset, blockSize);
        }
        offset += blockSize;
    }

    for(int i = 0; i < m; ++i) {
        ErlNifBinary tmpBlock;
        enif_alloc_binary(blockSize, &tmpBlock);
        codeBlocks[i] = (char*)tmpBlock.data;
        
        /*
        codeBlocks[i] = (char*)alloc(blockSize);

        BlockEntry tmpBlock;
        tmpBlock.data = (unsigned char*)codeBlocks[i];
        tmpBlock.size = blockSize;
        */

        allBlockEntry.push_back(tmpBlock);
    }

    jerasure_schedule_encode(k, m, w, smart, dataBlocks, codeBlocks, blockSize, blockSize / w);

    dealloc(dataBlocks);
    dealloc(codeBlocks);
    return allBlockEntry;
}

//BlockEntry CauchyCoding::doDecode(vector<BlockEntry> blockList, vector<int> blockIdList, size_t dataSize) {
ErlNifBinary CauchyCoding::doDecode(vector<ErlNifBinary> blockList, vector<int> blockIdList, size_t dataSize) {

    ErlNifBinary file;

    char** dataBlocks = (char**)alloc(sizeof(char*) * k);
    char** codeBlocks = (char**)alloc(sizeof(char*) * m);
    int erasures[k + m];
    size_t blockSize = blockList[0].size;
    set<int> availSet(blockIdList.begin(), blockIdList.end());

    int j = 0;
    for(int i = 0; i < k + m; ++i) {
        i < k ? dataBlocks[i] = (char*)alloc(blockSize) : codeBlocks[i - k] = (char*)alloc(blockSize);
        if (availSet.count(i) == 0) {
            erasures[j++] = i;
        }
    }
    erasures[j] = -1;

    for(size_t i = 0; i < blockList.size(); ++i) {
        int blockId = blockIdList[i];
        if (blockId < k) {
            memcpy(dataBlocks[blockId], blockList[i].data, blockSize);
        } else {
            memcpy(codeBlocks[blockId - k], blockList[i].data, blockSize);
        }
    }

    int *matrix = cauchy_good_general_coding_matrix(k, m, w);
    int *bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
    jerasure_schedule_decode_lazy(k, m, w, bitmatrix, erasures, dataBlocks, codeBlocks, blockSize, blockSize / w, 1);

    enif_alloc_binary(dataSize, &file);
    size_t offset = 0;
    int i = 0;
    while(offset < dataSize) {
        size_t copySize = min(dataSize - offset, blockSize);
        memcpy(file.data + offset, dataBlocks[i], copySize);
        i++;
        offset += copySize;
    }

    for(int i = 0; i < k + m; ++i) {
        i < k ? dealloc(dataBlocks[i]) : dealloc(codeBlocks[i - k]);
    }
    dealloc(dataBlocks);
    dealloc(codeBlocks);
    return file;
}
