#pragma once

#include "Constants.h"
#include "Logger.h"

#include <vector>
#include <algorithm>


template<typename T>
class InputsContainer
{
protected:

    // Mapping: index -> frame -> input
    std::vector<std::vector<T>> inputs;

    // Mapping: index -> frame -> boolean
    std::vector<std::vector<bool>> real;

    // // Last frame of input that changed
    // IndexedFrame lastChangedFrame = {{ UINT_MAX, UINT_MAX }};

public:

    bool fillFakeInputs = false;

    T get ( uint32_t index, uint32_t frame ) const
    {
        if ( index >= inputs.size() )
            return 0;

        if ( inputs[index].empty() )
            return 0;

        if ( frame >= inputs[index].size() )
            return inputs[index].back();

        return inputs[index][frame];
    }

    void get ( uint32_t index, uint32_t frame, T *t, size_t n ) const
    {
        ASSERT ( index < inputs.size() );
        ASSERT ( frame + n <= inputs[index].size() );

        std::copy ( inputs[index].begin() + frame,
                    inputs[index].begin() + frame + n, t );
    }

    void set ( uint32_t index, uint32_t frame, T t )
    {
        resize ( index, frame );

        inputs[index][frame] = t;
    }

    void set ( uint32_t index, uint32_t frame, T t, size_t n )
    {
        resize ( index, frame, n, true );

        std::fill ( inputs[index].begin() + frame,
                    inputs[index].begin() + frame + n, t );
    }

    void set ( uint32_t index, uint32_t frame, const T *t, size_t n )
    {
        // TODO refill inputs when faked inputs change

        // IndexedFrame f;
        // size_t i;

        // for ( i = 0, f = {{ frame, index }}; i < n; ++i, ++f.parts.frame )
        // {
        //     if ( get ( f.parts.index, f.parts.frame ) == t[i] )
        //         continue;

        //     // Indicate changed if the input is different from the last known input
        //     lastChangedFrame.value = std::min ( lastChangedFrame.value, f.value );
        //     break;
        // }

        resize ( index, frame, n, true );

        std::copy ( t, t + n, &inputs[index][frame] );
    }

    void resize ( uint32_t index, uint32_t frame, size_t n = 1, bool isReal = true )
    {
        if ( index >= inputs.size() )
            inputs.resize ( index + 1 );

        if ( frame + n > inputs[index].size() )
            inputs[index].resize ( frame + n, 0 );

        if ( fillFakeInputs )
        {
            if ( index >= real.size() )
                real.resize ( index + 1 );

            if ( frame + n > real[index].size() )
                real[index].resize ( frame + n, false );

            if ( isReal )
            {
                std::fill ( real[index].begin() + frame,
                            real[index].begin() + frame + n, true );
            }
        }
    }

    void clear()
    {
        inputs.clear();
        real.clear();
    }

    bool empty() const { return inputs.empty(); }

    bool empty ( size_t index ) const
    {
        if ( index + 1 > inputs.size() )
            return true;

        return inputs[index].empty();
    }

    uint32_t getEndIndex() const { return inputs.size(); }

    uint32_t getEndFrame() const
    {
        if ( inputs.empty() )
            return 0;

        return inputs.back().size();
    }

    // const IndexedFrame& getLastChangedFrame() const { return lastChangedFrame; }

    // void clearLastChangedFrame() { lastChangedFrame = {{ UINT_MAX, UINT_MAX }}; }
};
