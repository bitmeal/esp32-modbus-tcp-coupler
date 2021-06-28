#pragma once
#include <stdio.h>
#include <string.h>

void clear_stdin()
{
    while(fgetc(stdin) != EOF) { /*noop*/ };
}

void fgets_async_blocking(char* str, uint16_t size, FILE* fstr, bool echo, bool secure)
{
    char*  buffer = malloc(size*sizeof(char));
    memset(buffer, 0, size*sizeof(char));
    memset(str, 0, size*sizeof(char));
    // detect rollover by evaluating buffer[size-1] != 0
    clear_stdin();

    // bool ready = false;
    uint16_t ring_idx = 0;
    while(true)
    {
        int c;
        do
        {
            c = fgetc(fstr);
            // is char to append?
            if(c != EOF && c != '\n')
            {
                // "append" and increment ring buffer index
                buffer[ring_idx] = (char)c;
                ring_idx = (ring_idx + 1) % size;
                if(echo)
                {
                    printf("%c", secure ? '*' : (char)c);
                }
            }
        } while (c != EOF && c != '\n');
        
        // ready; found newline
        if(c == '\n')
        {
            break;
        }
        else
        {
            vTaskDelay(2*100 / portTICK_PERIOD_MS);
        }
    }

    // buffer/string is ready now
    // test for rollover and calculate size of ring buffer tail
    uint16_t tail_size = (buffer[size - 1] == 0) ? 0 : (size - ring_idx - 1);
    uint16_t head_size = ring_idx;

    if(tail_size)
    {
        memcpy(str, buffer + head_size + 1, tail_size * sizeof(char));
    }
    if(head_size)
    {
        memcpy(str + tail_size, buffer, head_size * sizeof(char));
    }
    str[head_size + tail_size] = 0;
    
    free(buffer);
}
// void fgets_async_blocking(uint16_t size, char* str) { fgets_async_blocking(size, str, stdin); }
