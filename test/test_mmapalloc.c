#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/mmapalloc.h"

int main() {
    /* try: allocate buat trigger mmap/virtualalloc utama  */
    void *init_arena = (void*)mmapalloc(sizeof(__uint8_t));
    if (!init_arena) {
        fprintf(stderr,"error: failed reserve\n");
        return -1;
    }

    /* masukin ke free list */
    mmapfree(init_arena);

    /* pemakaian
     * karena mmap/virtualalloc udah reserve memory besar
     * jadi di pakai buat bagi bagi tanpa harus panggil
     * mmap/virtualalloc berulang kali
     */

    char *buffer = (char *)mmapalloc(32 * sizeof(char));
    if (!buffer) {
        fprintf(stderr,"error: failed reserve memory\n");
        mmapalloc_destroy();
        return -1;
    }

    /* test: isi chunk dengan byte data */
    const char *text = "Test,Hello world";
    memcpy(buffer,text,strlen(text));
    fprintf(stdout,"%s\n",buffer);

    mmapfree(buffer);

    /* bebaskan semua arena + metadata */
    mmapalloc_destroy();
    return 0;
}