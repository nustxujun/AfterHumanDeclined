#ifndef _ring_H_
#define _ring_H_

#include <memory>

template<class Type>
class ring
{
public:
	struct AllocBuffer
	{
		void* buffer;
		size_t size;
	};
public:
	ring(size_t cap = 32) : mSize(0)
	{
		mCap = cap;
		mData = mFirst = mLast = (Type*)malloc(cap * sizeof(Type));
		mTail = mData + cap;
	}

	~ring()
	{
		delete mData;
		mData = mFirst = mLast = mTail = 0;
		mSize = 0;
	}

	void push_back(const Type& value)
	{
		if (mCap <= mSize)
		{
			reservce(mSize * 2);
		}

		new (mLast++) Type(value);
		if (mLast == mTail)
			mLast = mData;
		mSize++;

	}

	void pop_front()
	{
		if (mSize == 0)
			return;

		(mFirst++)->~Type();
		if (mFirst == mTail)
			mFirst = mData;
		mSize--;
	}

	Type* begin()
	{
		return mFirst;
	}


	size_t size()
	{
		return mSize;
	}


	bool empty()
	{
		return mSize == 0;
	}

	size_t reservce(size_t count)
	{
		if (count < mCap)
			return mCap;

		mCap = count;
		Type* temp = (Type*)malloc(count * sizeof(Type));
		if (mLast < mFirst || (mLast == mFirst && mSize != 0))
		{
			size_t size =  mTail - mFirst;
			memcpy(temp, mFirst, size * sizeof(Type));
			memcpy(temp + size, mData, sizeof(Type)* (mLast - mData));
		}
		else
		{ 
			memcpy(temp, mData, mSize * sizeof(Type));
		}

		free(mData);
		mData = mFirst = temp;
		mTail = mData + count;
		mLast = mFirst + mSize;
		return count;
	}
private:
	Type* mData;
	Type* mFirst;
	Type* mLast;
	Type* mTail;
	size_t mSize;
	size_t mCap;
};

#endif 