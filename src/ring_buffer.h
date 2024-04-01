/*
 * ring_buffer.h
 *
 * Created: 22-Okt-22
 *  Author: GuavTek
 */ 

// A simple, generalized ringbuffer object in C++

#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

template <uint8_t BUFFER_SIZE, typename T>
class ring_buffer_c
{
public:
	uint8_t Read(T* output);
	void Peek(T* output);
	void Write(T* in);
	uint8_t Count();
	void Flush();
	const uint8_t length = BUFFER_SIZE;
	//ring_buffer_c();
	
private:
	T buffer[BUFFER_SIZE];
	uint8_t head = 0;
	uint8_t tail = 0;
};

/*
template <uint8_t BUFFER_SIZE, typename T>
ring_buffer_c<BUFFER_SIZE, T>::ring_buffer_c(){
	tail = 0;
	head = 0;
	length = BUFFER_SIZE;
}
*/

//Read the next element in buffer
template <uint8_t BUFFER_SIZE, typename T>
uint8_t ring_buffer_c<BUFFER_SIZE, T>::Read(T* output){
	if (Count() > 0)
	{
		tail++;
		if (tail >= length)
		{
			tail = 0;
		}
		*output = buffer[tail];
		return 1;
	}
	return 0;
}

//Read next element without incrementing pointers
template <uint8_t BUFFER_SIZE, typename T>
void ring_buffer_c<BUFFER_SIZE, T>::Peek(T* output){
	uint8_t tempTail = tail + 1;
	
	if (tempTail >= length)
	{
		tempTail = 0;
	}
	
	*output = buffer[tempTail];
}

//Write an element to the buffer
template <uint8_t BUFFER_SIZE, typename T>
void ring_buffer_c<BUFFER_SIZE, T>::Write(T* in){
	if (Count() < length - 2)
	{
		head++;
		
		if (head >= length)
		{
			head = 0;
		}
		
		buffer[head] = *in;
	}
}

//Returns how many elements are in the buffer
template <uint8_t BUFFER_SIZE, typename T>
uint8_t ring_buffer_c<BUFFER_SIZE, T>::Count(){
	//Compensate for overflows
	if (head >= tail)
	{
		return (head - tail);
	} else {
		return (head - tail + length);
	}
}

//Resets buffer
template <uint8_t BUFFER_SIZE, typename T>
void ring_buffer_c<BUFFER_SIZE, T>::Flush(){
	tail = 0;
	head = 0;
}


#endif /* RING_BUFFER_H_ */