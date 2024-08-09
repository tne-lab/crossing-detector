/*
------------------------------------------------------------------

This file is part of a plugin for the Open Ephys GUI
Copyright (C) 2018 Translational NeuroEngineering Laboratory, MGH

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef CIRCULAR_ARRAY_H_INCLUDED
#define CIRCULAR_ARRAY_H_INCLUDED

/*
Extends (by ownership) the JUCE array type to use circular (modular) indices.
@see Array
*/

#include <BasicJuceHeader.h>

template <typename ElementType>
class CircularArray
{
public:
    /** Creates an empty circular array. */
    CircularArray() noexcept : start(0), isReset(true) {}

    /** Creates a circular array with a given length filled with default values 
        @param length   length of new array (if negative, new array is empty)
    */
    CircularArray(int length) : start(0), isReset(true)
    {
        array.resize(jmax(0, length));
    }

    ~CircularArray() {}

    /** Resets each element of the array to default value (without changing the size) */
    void reset()
    {
        int length = size();
        array.clearQuick();
        array.insertMultiple(0, ElementType(), length);
        start = 0;
        isReset = true;
    }

    /** Removes all elements from the array. */
    void clear()
    {
        array.clear();
        start = 0;
        isReset = true;
    }

    /** Returns number of elements in the array. */
    int size() const
    {
        return array.size();
    }

    /** Changes the size of the array by adding empty elements to or removing from the end
        @param targetNumItems   new array size
    */
    void resize(const int targetNumItems)
    {
        jassert(targetNumItems >= 0);
        int length = size();
        if (targetNumItems == 0)
        {
            clear();
        }
        else if (targetNumItems > length)
        {
            insertMultiple(length, ElementType(), targetNumItems - length);
        }
        else // 0 < targetNumItems <= length
        {
            removeLast(length - targetNumItems);
        }
    }

    /** Returns one of the elements in the array.
        @param index    circular index of requested element
        @return         requested element, or default value if array is empty
    */
    ElementType operator[](const int index) const
    {
        return size() > 0 ? array[circToLinInd(index)] : ElementType();
    }

    /** Replaces an element with a new value.
        Note that because of the circular nature of the array, this never increases the size
        of the array. If the array is empty, this is a no-op.
        @param indexToChange    index of element to replace
        @param newValue         element to replace it with
    */
    void set(const int indexToChange, ElementType newValue)
    {
        if (size() > 0)
        {
            array.set(circToLinInd(indexToChange), newValue);
            if (newValue != ElementType())
            {
                isReset = false;
            }
        }
    }

    /** Adds a new element at the end of the array, overwriting the previous first element.
        Does not change the array size and does nothing if the array is empty.
        @param newElement      new element to enqueue
    */
    void enqueue(ElementType newValue)
    {
        set(0, newValue);
        start = mod(start + 1, size());
        isReset = false;
    }

    /** Adds the n = min(numberOfElements, size()) last elements of the input array, overwriting
        the n first (oldest) elements currently in the array.
        @param newValues            start of array to add
        @param numberOfElements     size of array to add
    */
    void enqueueArray(const ElementType* newValues, int numberOfElements)
    {
        int length = size();
        int n = jmin(numberOfElements, length);
        int nToSkip = numberOfElements - n;
        int nFirstSegment = jmin(n, length - start);
        int nSecondSegment = n - nFirstSegment;

        for (int i = 0; i < nFirstSegment; ++i)
        {
            array.set(start + i, newValues[nToSkip + i]);
        }

        for (int i = 0; i < nSecondSegment; ++i)
        {
            array.set(i, newValues[nToSkip + nFirstSegment + i]);
        }

        start = mod(start + n, length);
        isReset = false;
    }

    /** Inserts multiple copies of an element into the array at a given position (lengthening
        the array). If the index is less than zero or greater than the size of the array, the
        elements will be inserted at the end of the array.
        @param indexToInsertAt      the index at which the new elements should be inserted
        @param newElement           the new object to add to the array
        @numberOfTimesToInsertIt    how many copies of the value to insert
    */
    void insertMultiple(int indexToInsertAt, ElementType newElement, int numberOfTimesToInsertIt)
    {
        if (numberOfTimesToInsertIt > 0)
        {
            int length = size();

            if (indexToInsertAt < 0 || indexToInsertAt >= length)
            {
                indexToInsertAt = length;
            }

            int linIndexToInsertAt;
            if (length > 0)
            {
                if (isReset)
                {
                    // change 'start' to avoid moving elements
                    start = (length - indexToInsertAt) % length;
                }

                // get index to insert at in range [1, length]
                linIndexToInsertAt = circToLinInd(indexToInsertAt - 1) + 1;
            }
            else
            {
                linIndexToInsertAt = 0;
            }

            array.insertMultiple(linIndexToInsertAt, newElement, numberOfTimesToInsertIt);

            // move start to follow first element if it was moved and we're not inserting at 0
            if (linIndexToInsertAt <= start && indexToInsertAt > 0)
            {
                start += numberOfTimesToInsertIt;
            }

            // if inserting at 0 and taking advantage of buffer circularity, point to start of inserted data
            if (indexToInsertAt == 0 && start == 0)
            {
                start += length;
            }

            if (newElement != ElementType())
            {
                isReset = false;
            }
        }
    }

    /** Removes the last n elements from the array.
        @param howManyToRemove  how many elements to remove
    */
    void removeLast(int howManyToRemove = 1)
    {
        if (howManyToRemove > 0)
        {
            int length = size();

            // special cases
            if (howManyToRemove >= length)
            {
                clear();
                return;
            }

            if (isReset)
            {
                start = 0;
                array.removeLast(howManyToRemove);
                return;
            }

            // howManyToRemove < length and we can't move start.

            int numRemoveFromStart = jmin(start, howManyToRemove);
            int numRemoveFromEnd = howManyToRemove - numRemoveFromStart;

            array.removeLast(numRemoveFromEnd);
            array.removeRange(start - numRemoveFromStart, numRemoveFromStart);
            start -= numRemoveFromStart;
        }
    }

private:
    // precondition: array is nonempty.
    int circToLinInd(int index) const
    {
        return mod(start + index, size());
    }

    static int mod(int x, int m)
    {
        jassert(m > 0);
        return (x % m + m) % m;
    }

    Array<ElementType> array;
    int start; // index of the start of the array
    bool isReset; // whether all elements are default
};

#endif // CIRCULAR_ARRAY_H_INCLUDED