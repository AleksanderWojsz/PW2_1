#include <stdbool.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mimpi_common.h"

#ifndef PW2_MOJA_H
#define PW2_MOJA_H

#define MAX_PATH_LENGTH 1024


#include <stdlib.h>
#include <string.h>




// Znajdowanie pierwszego elementu o wskazanym count, source, tag (lub tag == 0)
Message *list_find_with_last(Message *head, int count, int source, int tag, Message **last) {
    Message *current = head;
    while (current != NULL) {
        if ((current->count == count && current->source == source && (current->tag == tag || current->tag == 0)) ||
            (current->source == source && current->tag < 0)) { // Wiadomość specjalna od konkretnego nadawcy

            *last = current;
            return current;
        }
        current = current->next;
    }

    *last = NULL;
    return NULL; // Nie znaleziono elementu
}


void print_open_descriptors(void)
{
    const char* path = "/proc/self/fd";

    // Iterate over all symlinks in `path`.
    // They represent open file descriptors of our process.
    DIR* dr = opendir(path);
    if (dr == NULL)
        fatal("Could not open dir: %s", path);

    struct dirent* entry;
    while ((entry = readdir(dr)) != NULL) {
        if (entry->d_type != DT_LNK)
            continue;

        // Make a c-string with the full path of the entry.
        char subpath[MAX_PATH_LENGTH];
        int ret = snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
        if (ret < 0 || ret >= (int)sizeof(subpath))
            fatal("Error in snprintf");

        // Read what the symlink points to.
        char symlink_target[MAX_PATH_LENGTH];
        ssize_t ret2 = readlink(subpath, symlink_target, sizeof(symlink_target) - 1);
        ASSERT_SYS_OK(ret2);
        symlink_target[ret2] = '\0';

        // Skip an additional open descriptor to `path` that we have until closedir().
        if (strncmp(symlink_target, "/proc", 5) == 0)
            continue;

        fprintf(stderr, "Pid %d file descriptor %3s -> %s\n",
                getpid(), entry->d_name, symlink_target);
    }

    fprintf(stderr, "\n\n");

    closedir(dr);
}


void list_print(Message *head) {
    Message *current = head;
    int i = 0; // Licznik do numerowania wiadomości

    while (current != NULL) {
        printf("Wiadomosc %d:\n", ++i);
        if (current->data != NULL) {
            printf("  data %d\n", *(const int *) current->data);
        } else {
            printf("  data NULL\n");
        }
        printf("  Ilosc: %d\n", current->count);
        printf("  Zrodlo: %d\n", current->source);
        printf("  Tag: %d\n\n", current->tag);
        current = current->next;
    }
}


#endif //PW2_MOJA_H
