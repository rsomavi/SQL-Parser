#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024

void read_table(const char *table) {
    char filename[256];
    sprintf(filename, "../data/%s.tbl", table);
    
    FILE *file = fopen(filename, "r");
    
    if (!file) {
        fprintf(stderr, "ERROR: table not found\n");
        exit(1);
    }
    
    char line[MAX_LINE];
    
    while (fgets(line, MAX_LINE, file)) {
        printf("%s", line);
    }
    
    fclose(file);
}

int main(int argc, char *argv[]) {
    
    if (argc < 3) {
        fprintf(stderr, "Usage: disk read <table>\n");
        return 1;
    }
    
    if (strcmp(argv[1], "read") == 0) {
        read_table(argv[2]);
    }
    
    return 0;
}
