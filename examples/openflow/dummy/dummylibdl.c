typedef struct {
   const char *dli_fname;  /* Pathname of shared object that contains address */
   void       *dli_fbase;  /* Address at which shared object is loaded */
   const char *dli_sname;  /* Name of nearest symbol with address lower than addr */
   void       *dli_saddr;  /* Exact address of symbol named in dli_sname */
} Dl_info;

void *dlopen(const char *filename, int flag) {
   return (void*)0;
}

char *dlerror(void) {
   return (char*)0;
}

void *dlsym(void *handle, const char *symbol) {
   return (void*)0;
}

int dlclose(void *handle) {
   return 0;
}

int dladdr(void *addr, Dl_info *info) {
   return 0;
}

