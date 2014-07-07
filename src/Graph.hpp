#ifndef GRAPH_H
#define GRAPH_H

#include "Common.hpp"
#include <omp.h>
#include <unordered_map>
#include <xmmintrin.h>
#include <cstring>
#include <immintrin.h>
#include <unordered_map>


#define SHORTS_PER_REG 8

using namespace std;
long numSets = 0;
long numSetsCompressed = 0;

int getBit(int value, int position) {
  return ( ( value & (1 << position) ) >> position);
}

inline size_t simd_intersect_vector16(const size_t lim,const unsigned short *A, const unsigned short *B, const size_t s_a, const size_t s_b) {
  size_t count = 0;
  size_t i_a = 0, i_b = 0;

  size_t st_a = (s_a / SHORTS_PER_REG) * SHORTS_PER_REG;
  size_t st_b = (s_b / SHORTS_PER_REG) * SHORTS_PER_REG;
  //scout << "Sizes:: " << st_a << " " << st_b << endl;
 
  while(i_a < st_a && i_b < st_b && A[i_a+SHORTS_PER_REG-1] < lim && B[i_b+SHORTS_PER_REG-1] < lim) {
    __m128i v_a = _mm_loadu_si128((__m128i*)&A[i_a]);
    __m128i v_b = _mm_loadu_si128((__m128i*)&B[i_b]);    
    unsigned short a_max = _mm_extract_epi16(v_a, SHORTS_PER_REG-1);
    unsigned short b_max = _mm_extract_epi16(v_b, SHORTS_PER_REG-1);
    
    __m128i res_v = _mm_cmpestrm(v_b, SHORTS_PER_REG, v_a, SHORTS_PER_REG,
            _SIDD_UWORD_OPS|_SIDD_CMP_EQUAL_ANY|_SIDD_BIT_MASK);
    int r = _mm_extract_epi32(res_v, 0);
    //__m128i p = _mm_shuffle_epi8(v_a, shuffle_mask16[r]);
    //_mm_storeu_si128((__m128i*)&C[count], p);
    count += _mm_popcnt_u32(r);
    
    i_a += (a_max <= b_max) * SHORTS_PER_REG;
    i_b += (a_max >= b_max) * SHORTS_PER_REG;
  }
  // intersect the tail using scalar intersection
  //...

  bool notFinished = i_a < s_a  && i_b < s_b && A[i_a] < lim && B[i_b] < lim;
  while(notFinished){
    while(notFinished && B[i_b] < A[i_a]){
      ++i_b;
      notFinished = i_b < s_b && B[i_b] < lim;
    }
    if(notFinished && A[i_a] == B[i_b]){
     ++count;
    }
    ++i_a;
    notFinished = notFinished && i_a < s_a && A[i_a] < lim;
  }

  return count;
}

// A, B - partitioned operands
inline size_t intersect_partitioned(const size_t lim,const unsigned short *A, const unsigned short *B, const size_t s_a, const size_t s_b) {
  size_t i_a = 0, i_b = 0;
  size_t counter = 0;
  size_t limPrefix = (lim & 0x0FFFF0000) >> 16;
  size_t limLower = lim & 0x0FFFF;
  bool notFinished = i_a < s_a && i_b < s_b && A[i_a] <= limPrefix && B[i_b] <= limPrefix;

  //cout << lim << endl;
  while(notFinished) {
    //size_t limLower = limLowerHolder;
    //cout << "looping" << endl;
    if(A[i_a] < B[i_b]) {
      i_a += A[i_a + 1] + 2;
      notFinished = i_a < s_a && A[i_a] <= limPrefix;
    } else if(B[i_b] < A[i_a]) {
      i_b += B[i_b + 1] + 2;
      notFinished = i_b < s_b && B[i_b] <= limPrefix;
    } else {
      unsigned short partition_size;
      //If we are not in the range of the limit we don't need to worry about it.
      if(A[i_a] < limPrefix && B[i_b] < limPrefix){
        partition_size = simd_intersect_vector16(10000000,&A[i_a + 2], &B[i_b + 2],A[i_a + 1], B[i_b + 1]);
      } else {
        partition_size = simd_intersect_vector16(limLower,&A[i_a + 2], &B[i_b + 2],A[i_a + 1], B[i_b + 1]);
      }
      counter += partition_size;
      i_a += A[i_a + 1] + 2;
      i_b += B[i_b + 1] + 2;
      notFinished = i_a < s_a && i_b < s_b && A[i_a] <= lim && B[i_b] <= limPrefix;
    }
  }
  return counter;
}
void print_partition(unsigned short *A, size_t s_a){
  for(size_t i = 0; i < s_a; i++){
    unsigned int prefix = (A[i] << 16);
    unsigned short size = A[i+1];
    cout << "size: " << size << endl;
    i += 2;
    size_t inner_end = i+size;
    while(i < inner_end){
      unsigned int tmp = prefix | A[i];
      cout << prefix << " " << tmp << endl;
      ++i;
    }
    i--;
  }
}

// A - sorted array
// s_a - size of A
// R - partitioned sorted array
inline size_t partition(int *A, size_t s_a, unsigned short *R, size_t index) {
  unsigned short high = 0;
  size_t partition_length = 0;
  size_t partition_size_position = index+1;
  size_t counter = index;
  for(size_t p = 0; p < s_a; p++) {
      unsigned short chigh = (A[p] & 0xFFFF0000) >> 16; // upper dword
      unsigned short clow = A[p] & 0x0FFFF;   // lower dword
      if(chigh == high && p != 0) { // add element to the current partition
        R[counter++] = clow;
        partition_length++;
      } else { // start new partition
        R[counter++] = chigh; // partition prefix
        R[counter++] = 0;     // reserve place for partition size
        R[counter++] = clow;  // write the first element
        R[partition_size_position] = partition_length;
        numSets++;
        if(partition_length > 4096)
          numSetsCompressed++;
        //cout << "setting: " << partition_size_position << " to: " << partition_length << endl;
        partition_length = 1; // reset counters
        partition_size_position = counter - 2;
        high = chigh;
      }
  }
  R[partition_size_position] = partition_length;

  return counter;
}
struct CompressedGraph {
  const size_t num_nodes;
  const size_t num_edges;
  const size_t *nbr_lengths;
  const size_t *nodes;
  unsigned short *edges;
  const unordered_map<size_t,size_t> *external_ids;
  CompressedGraph(  
    const size_t num_nodes_in, 
    const size_t num_edges_in,
    const size_t *nbrs_lengths_in, 
    const size_t *nodes_in,
    unsigned short *edges_in,
    const unordered_map<size_t,size_t> *external_ids_in): 
      num_nodes(num_nodes_in), 
      num_edges(num_edges_in),
      nbr_lengths(nbrs_lengths_in),
      nodes(nodes_in), 
      edges(edges_in),
      external_ids(external_ids_in){}

    inline void printGraph(){
      for(size_t i = 0; i < num_nodes; ++i){
        size_t start1 = nodes[i];
        
        size_t end1 = 0;
        if(i+1 < num_nodes) end1 = nodes[i+1];
        else end1 = num_edges;

        size_t j = start1;
        cout << "Node: " << i <<endl;

        while(j < end1){
          unsigned int prefix = (edges[j] << 16);
          size_t inner_end = j+edges[j+1]+2;
          j += 2;
          while(j < inner_end){
            size_t cur = prefix | edges[j];
            cout << "nbr: " << cur << endl;
            ++j;
          }
        }
        cout << endl;
      }
    }
    inline double pagerank(){
      double *pr = new double[num_nodes];
      double *oldpr = new double[num_nodes];
      const double damp = 0.85;
      const int maxIter = 100;
      const double threshold = 0.0001;

      for(size_t i=0; i < num_nodes; ++i){
        oldpr[i] = 1.0/num_nodes;
      }

      //omp_set_num_threads(numThreads);
      //#pragma omp parallel for default(none) schedule(static,150) reduction(+:result)        
      int iter = 0;
      double delta = 1000000000000.0;
      double totalpr = 0.0;
      while(delta > threshold && iter < maxIter){
        totalpr = 0.0;
        delta = 0.0;
        for(size_t i = 0; i < num_nodes; ++i){
          size_t start1 = nodes[i];
          
          size_t end1 = 0;
          if(i+1 < num_nodes) end1 = nodes[i+1];
          else end1 = num_edges;

          size_t j = start1;
          double sum = 0.0;
          while(j < end1){
            unsigned int prefix = (edges[j] << 16);
            size_t inner_end = j+edges[j+1]+2;
            j += 2;
            while(j < inner_end){
              size_t cur = prefix | edges[j];
              size_t len2 = nbr_lengths[cur];
              sum += oldpr[cur]/len2;
              ++j;
            }
          }
          pr[i] = ((1.0-damp)/num_nodes) + damp * sum;
          delta += abs(pr[i]-oldpr[i]);
          totalpr += pr[i];
        }

        double *tmp = oldpr;
        oldpr = pr;
        pr = tmp;
        ++iter;
      }
      pr = oldpr;
    
      /*
      cout << "Iter: " << iter << endl;
      for(size_t i=0; i < num_nodes; ++i){
        cout << "Node: " << i << " PR: " << pr[i] << endl;
      }
      */

      return 1.0;
    }   
    inline long countTriangles(int numThreads){
      cout << "COMPRESSED EDGE BYTES: " << (num_edges * 16)/8 << endl;

      long count = 0;
      omp_set_num_threads(numThreads);  
      #pragma omp parallel for default(none) schedule(static,150) reduction(+:count)   
      for(size_t i = 0; i < num_nodes; ++i){
        const size_t start1 = nodes[i];
        
        size_t end1 = 0;
        if(i+1 < num_nodes) end1 = nodes[i+1];
        else end1 = num_edges;
        const size_t len1 = end1-start1;

        size_t j = start1;

        while(j < end1){
          const size_t prefix = (edges[j] << 16);
          const size_t inner_end = j+edges[j+1]+2l;
          j += 2;

          bool notFinished = (i >> 16) >= edges[j-2];
          //cout << "Prefixes: " << (i >> 16) << " and " << edges[j-2] << endl;

          while(j < inner_end && notFinished){
            const size_t cur = prefix | edges[j];
              //cout << "Here: " << i << " and " << cur << endl;

            const size_t start2 = nodes[cur];
            size_t end2 = 0;
            if(cur+1 < num_nodes) end2 = nodes[cur+1];
            else end2 = num_edges;
            const size_t len2 = end2-start2;

            notFinished = i > cur; //has to be reverse cause cutoff could
            //be in the middle of a partition.
            if(notFinished){
              long ncount = intersect_partitioned(cur,edges+start1,edges+start2,len1,len2);
              count += ncount;
            }
            ++j;
          }
          j = inner_end;
        }
      }
      return count;
    }
};
static inline CompressedGraph* createCompressedGraph (VectorGraph *vg) {
  size_t *nodes = new size_t[vg->num_nodes];
  size_t *nbrlengths = new size_t[vg->num_nodes];
  unsigned short *edges = new unsigned short[vg->num_edges*5];
  size_t num_nodes = vg->num_nodes;
  const unordered_map<size_t,size_t> *external_ids = vg->external_ids;

  //cout  << "Num nodes: " << vg->num_nodes << " Num edges: " << vg->num_edges << endl;

  size_t index = 0;
  for(size_t i = 0; i < vg->num_nodes; ++i){
    vector<size_t> *hood = vg->neighborhoods->at(i);
    nbrlengths[i] = hood->size();
    int *tmp_hood = new int[hood->size()];
    for(size_t j = 0; j < hood->size(); ++j) {
      tmp_hood[j] = hood->at(j);
    }
    nodes[i] = index;
    index = partition(tmp_hood,hood->size(),edges,index);
    delete[] tmp_hood;
  }

  cout << "num sets: " << numSets << " numSetsCompressed: " << numSetsCompressed << endl;

  return new CompressedGraph(num_nodes,index,nbrlengths,nodes,edges,external_ids);
}

#endif
