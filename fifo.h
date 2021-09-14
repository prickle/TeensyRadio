#ifndef _FIFO_h_
#define _FIFO_h_


// 180Kb streaming fifo buffer

 
#define FIFO_SIZE 1024*180 

class Fifo
{
  public:
    Fifo(void) {
      size = FIFO_SIZE;
      fifodata = (uint8_t*)malloc(size);
      reset();
    }
    void reset(void) {
       head = tail = count = 0;
    }
    inline size_t available(void) {
      return FIFO_SIZE - count;
    }
    inline size_t used(void) {
      return count;
    }    
    int write(uint8_t* buf, size_t length);
    int read(uint8_t* buf, size_t length);
   
  private:    
    size_t size, head, tail, count;
    uint8_t* fifodata;
};

  
int Fifo::write(uint8_t* buf, size_t length)
{
  if (available() < length) return -1;
  
  count += length;  
  int len = min(length, size - head);
  memcpy(fifodata + head, buf, len);  
  head +=len;
  buf += len;
  len = length - len;
  if (len) {
    memcpy(fifodata, buf, len);
    head = len;
  }

  return length;
}

int Fifo::read(uint8_t* buf, size_t length)
{
  if (count < length) length=count;     
  count -= length;    
  int len = min(length, size - tail);
  memcpy(buf, fifodata + tail, len);  
  tail +=len;
  buf += len;
  len = length - len;
  if (len) {
    memcpy(buf, fifodata, len);
    tail = len;
  }  
//  if (count==0) tail=head=0;//optimization
  return length;
}

#endif
