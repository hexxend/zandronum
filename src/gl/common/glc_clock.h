#ifndef __GL_CLOCK_H
#define __GL_CLOCK_H

#include "stats.h"
#include "x86.h"

#ifdef _MSC_VER

extern double gl_SecondsPerCycle;
extern double gl_MillisecPerCycle;


inline long long GetClockCycle ()
{
#if _M_X64
	return __rdtsc();
#else
	return CPU.bRDTSC ? __rdtsc() : 0;
#endif
}

#elif defined(__GNUG__) && defined(__i386__)

extern double gl_SecondsPerCycle;
extern double gl_MillisecPerCycle;

inline long long GetClockCycle()
{
	if (CPU.bRDTSC)
	{
		long long res;
		asm volatile ("rdtsc" : "=A" (res));
		return res;
	}
	else
	{
		return 0;
	}	
}

#else

extern double gl_SecondsPerCycle;
extern double gl_MillisecPerCycle;

inline long long GetClockCycle ()
{
	return 0;
}
#endif

class glcycle_t
{
public:
	glcycle_t &operator= (const glcycle_t &o)
	{
		Counter = o.Counter;
		return *this;
	}

	void Reset()
	{
		Counter = 0;
	}
	
	void Clock()
	{
		// Not using QueryPerformanceCounter directly, so we don't need
		// to pull in the Windows headers for every single file that
		// wants to do some profiling.
		long long time = GetClockCycle();
		Counter -= time;
	}
	
	void Unclock()
	{
		long long time = GetClockCycle();
		Counter += time;
	}
	
	double Time()
	{
		return double(Counter) * gl_SecondsPerCycle;
	}
	
	double TimeMS()
	{
		return double(Counter) * gl_MillisecPerCycle;
	}

private:
	long long Counter;
};


extern glcycle_t RenderWall,SetupWall,ClipWall;
extern glcycle_t RenderFlat,SetupFlat;
extern glcycle_t RenderSprite,SetupSprite;
extern glcycle_t All, Finish, PortalAll;
extern int vertexcount, flatvertices, flatprimitives;


#endif