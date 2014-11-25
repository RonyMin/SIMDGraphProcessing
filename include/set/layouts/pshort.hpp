/*

THIS CLASS IMPLEMENTS THE FUNCTIONS ASSOCIATED WITH A PREFIX SHORT SET LAYOUT.

*/

#include "common.hpp"
#include "../ops/intersection.hpp"

class pshort{
  public:
    static common::type get_type();
    static size_t build(uint8_t *r_in, const uint32_t *data, const size_t length);
    static size_t build_flattened(uint8_t *r_in, const uint32_t *data, const size_t length);
    static tuple<size_t,size_t,common::type> get_flattened_data(const uint8_t *set_data, const size_t cardinality);
   
    static void foreach(const std::function <void (uint32_t)>& f,
      const uint8_t *data_in, 
      const size_t cardinality, 
      const size_t number_of_bytes,
      const common::type type);
    
    static tuple<size_t,size_t,common::type> intersect(uint8_t *C_in, 
      const uint8_t *A_in, const uint8_t *B_in, 
      const size_t A_cardinality, const size_t B_cardinality, 
      const size_t A_num_bytes, const size_t B_num_bytes, 
      const common::type a_t, const common::type b_t);
};

inline common::type pshort::get_type(){
  return common::PSHORT;
}
//Copies data from input array of ints to our set data r_in
inline size_t pshort::build(uint8_t *r_in, const uint32_t *A, const size_t s_a){
  uint16_t *R = (uint16_t*) r_in;

  uint16_t high = 0;
  size_t partition_length = 0;
  size_t partition_size_position = 1;
  size_t counter = 0;
  for(size_t p = 0; p < s_a; p++) {
    uint16_t chigh = (A[p] & 0xFFFF0000) >> 16; // upper dword
    uint16_t clow = A[p] & 0x0FFFF;   // lower dword
    if(chigh == high && p != 0) { // add element to the current partition
      partition_length++;
      R[counter++] = clow;
    }else{ // start new partition
      R[counter++] = chigh; // partition prefix
      R[counter++] = 0;     // reserve place for partition size
      R[counter++] = clow;  // write the first element
      R[partition_size_position] = partition_length;

      partition_length = 1; // reset counters
      partition_size_position = counter - 2;
      high = chigh;
    }
  }
  R[partition_size_position] = partition_length;
  return counter*2;
}
//Nothing is different about build flattened here. The number of bytes
//can be infered from the type. This gives us back a true CSR representation.
inline size_t pshort::build_flattened(uint8_t *r_in, const uint32_t *data, const size_t length){
  if(length > 0){
    size_t *size_ptr = (size_t*) r_in;
    size_t num_bytes = build(r_in+sizeof(size_t),data,length);
    size_ptr[0] = num_bytes;
    return num_bytes+sizeof(size_t);
  } else{
    return 0;
  }
}

inline tuple<size_t,size_t,common::type> pshort::get_flattened_data(const uint8_t *set_data, const size_t cardinality){
  if(cardinality > 0){
    const size_t *size_ptr = (size_t*) set_data;
    return make_tuple(sizeof(size_t),size_ptr[0],common::PSHORT);
  } else{
    return make_tuple(0,0,common::PSHORT);
  }
}

//Iterates over set applying a lambda.
inline void pshort::foreach(const std::function <void (uint32_t)>& f, 
  const uint8_t *A_in, 
  const size_t cardinality, 
  const size_t number_of_bytes, 
  const common::type type){
  (void) number_of_bytes; (void) type;

  uint16_t *A = (uint16_t*) A_in;
  size_t count = 0;
  size_t i = 0;
  while(count < cardinality){
    uint32_t prefix = (A[i] << 16);
    unsigned short size = A[i+1];
    i += 2;

    size_t inner_end = i+size;
    while(i < inner_end){
      uint32_t tmp = prefix | A[i];
      f(tmp);
      ++count;
      ++i;
    }
  }
}

inline tuple<size_t,size_t,common::type> pshort::intersect(uint8_t *C_in, 
  const uint8_t *A_in, const uint8_t *B_in, 
  const size_t card_a, const size_t card_b, 
  const size_t s_bytes_a, const size_t s_bytes_b, 
  const common::type a_t, const common::type b_t) {
  (void) card_a; (void) card_b; (void) a_t; (void) b_t;
  
  return ops::intersect_pshort_pshort((uint16_t*)C_in,(uint16_t*)A_in,(uint16_t*)B_in,s_bytes_a/sizeof(uint16_t),s_bytes_b/sizeof(uint16_t));
}
