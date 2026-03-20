/*
**	Command & Conquer(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header:   F:\projects\c&c\vcs\code\jshell.h_v   2.16   16 Oct 1995 16:45:06   JOE_BOSTIC  $ */
/*********************************************************************************************** 
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               *** 
 *********************************************************************************************** 
 *                                                                                             * 
 *                 Project Name : Command & Conquer                                            * 
 *                                                                                             * 
 *                    File Name : JSHELL.H                                                     * 
 *                                                                                             * 
 *                   Programmer : Joe L. Bostic                                                * 
 *                                                                                             * 
 *                   Start Date : 03/13/95                                                     * 
 *                                                                                             * 
 *                  Last Update : March 13, 1995 [JLB]                                         * 
 *                                                                                             * 
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef JSHELL_H
#define JSHELL_H

/*
**	Interface class to the keyboard. This insulates the game from library vagaries. Most
**	notable being the return values are declared as "int" in the library whereas C&C
**	expects it to be of KeyNumType.
*/
class Keyboard 
{
	public:
		static KeyNumType Get(void) {return (KeyNumType)Get_Key_Num();};
		static KeyNumType Check(void) {return (KeyNumType)Check_Key_Num();};
		static KeyASCIIType To_ASCII(KeyNumType key) {return (KeyASCIIType)KN_To_KA(key);};
		static void Clear(void) {Clear_KeyBuffer();};
		static void Stuff(KeyNumType key) {Stuff_Key_Num(key);};
		static int Down(KeyNumType key) {return Key_Down(key);};
		static int Mouse_X(void) {return Get_Mouse_X();};
		static int Mouse_Y(void) {return Get_Mouse_Y();};
};


#ifdef NEVER
inline void * operator delete(void * data) 
{
	Free(data);
}

inline void * operator delete[] (void * data)
{
	Free(data);
}
#endif


/*
**	These templates allow enumeration types to have simple bitwise
**	arithmatic performed. The operators must be instatiated for the
**	enumerated types desired.
*/
template<class T> inline T operator ++(T & a)
{
	a = (T)((int)a + (int)1);
	return(a);
}
template<class T> inline T operator ++(T & a, int)
{
	T aa = a;
	a = (T)((int)a + (int)1);
	return(aa);
}	
template<class T> inline T operator --(T & a)
{
	a = (T)((int)a - (int)1);
	return(a);
}
template<class T> inline T operator --(T & a, int)
{
	T aa = a;
	a = (T)((int)a - (int)1);
	return(aa);
}
template<class T> inline T operator |(T t1, T t2)
{
	return((T)((int)t1 | (int)t2));
}
template<class T> inline T operator &(T t1, T t2)
{
	return((T)((int)t1 & (int)t2));
}
template<class T> inline T operator ~(T t1)
{
	return((T)(~(int)t1));
}


/*
**	The shape flags are likely to be "or"ed together and other such bitwise
**	manipulations. These instatiated operator templates allow this.
*/
/* ShapeFlags_Type operators provided by templates above */


void Set_Bit(void * array, int bit, int value);
int Get_Bit(void const * array, int bit);
int First_True_Bit(void const * array);
int First_False_Bit(void const * array);
extern int Bound(int original, int min, int max);
#ifdef NEVER
extern unsigned Bound(unsigned original, unsigned min, unsigned max);
#endif

unsigned Fixed_To_Cardinal(unsigned base, unsigned fixed);
unsigned Cardinal_To_Fixed(unsigned base, unsigned cardinal);

#endif /* JSHELL_H */
