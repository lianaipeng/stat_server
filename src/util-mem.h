

#define DELETE_IF_NOT_NULL(a) { \
    if(a != NULL){ \
        delete a; \
        a = NULL; \
    } \
}
