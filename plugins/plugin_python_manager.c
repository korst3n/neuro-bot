#include "plugin_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <dlfcn.h>

#define PLUGIN_DIR "plugins"

/*
 * Python library functions. Should match the prototypes in python lib headers.
 */

typedef void *      PyObject;
static  void        (*Py_Initialize)            (void);
static  void        (*PyList_Append)            (PyObject *, PyObject *);
static  PyObject    (*PySys_GetObject)          (char *);
static  PyObject    (*PyString_FromString)      (char *);
static  PyObject    (*PyImport_ImportModule)    (char *);
static  PyObject    (*PyObject_GetAttrString)   (PyObject *, char *);
static  PyObject    (*PyImport_AddModule)       (char *);
static  int         (*PyTuple_SetItem)          (PyObject *, size_t pos, PyObject *);
static  int         (*PyCallable_Check)         (PyObject *);
static  PyObject    (*PyTuple_New)              (int);
static  PyObject    (*PyObject_CallObject)      (PyObject *, PyObject *);
static  char *      (*PyString_AsString)        (PyObject *);

/**
 * Internal python plugins struct for each python plugin as all managed by this plugin. 
 */
struct py_module_t {
    char * name;

    int is_command;
    int is_grep;
    int is_looper;

    PyObject * pName, * pModule, * pFunc;
};

static struct python_plugin_list_t * head;

struct python_plugin_list_t {
    struct py_module_t      * cur;
    struct python_plugin_list_t    * next;
};

static void insert(struct py_module_t * p)
{
    if (head == NULL) {
        head = malloc(sizeof (struct python_plugin_list_t));
        head->cur = p;
        head->next = NULL;
    } else {
        struct python_plugin_list_t * it;

        for (it = head; it->next != NULL; it = it->next) {}

        it->next = malloc(sizeof (struct python_plugin_list_t));
        it->next->cur = p;
        it->next->next = NULL;
    }
}

static void plugin_load_file(char * fpath)
{
    struct py_module_t * mod;
    char mod_name[50];
    char mod_command_name[50];
    char * file_name;
    int k;

    if (!fpath) {
        fprintf(stderr, "%25s:%4d:plugin_load_file: fpath cannot be null.\n", __FILE__, __LINE__);
        return;
    }

    if (strcmp(fpath+strlen(fpath)-3, ".py"))
        return;

    strncpy(mod_name, strchr(fpath, '/')+1, strcspn(fpath, "."));

    mod = malloc(sizeof (struct py_module_t));

    mod->pName = PyString_FromString(mod_name);
    *strchr(mod_name, '.') = 0;
    mod->pModule = PyImport_ImportModule(mod_name);

    if (!mod->pModule) {
        fprintf(stderr, "%25s:%4d:Can't load module: %s\n", __FILE__, __LINE__, fpath);
        free(mod);
        return;
    }

    mod->pFunc = PyObject_GetAttrString(mod->pModule, mod_name);

    if (!mod->pFunc || !PyCallable_Check(mod->pFunc)) {
        fprintf(stderr, "%25s:%4d:Error python call method check for module %s and attr %s.\n", __FILE__, __LINE__, 
                        fpath, mod_name);
        free(mod);
        return;
    }

    mod_command_name[0] = 0;
    strncpy(mod_command_name, strchr(mod_name, '_')+1, strcspn(mod_name, ".")); 
    mod->name = strdup(mod_command_name);
    mod->is_command = 1;
    insert(mod);

    fprintf(stderr, "%25s:%4d:Python module loaded: [%s]\n", __FILE__, __LINE__, fpath);
}

static void load_python_plugins()
{
    DIR * dir;
    struct dirent * dirent;

    dir = opendir(PLUGIN_DIR);

    if (!dir)
    {
        fprintf(stderr, "%25s:%4d: no modules found, skipping.\n", __FILE__, __LINE__);
        return;
    }

    while ((dirent = readdir(dir)) != NULL)
    {
        if (strstr(dirent->d_name, ".py")) {
            char plugin_path[200];
            plugin_path[0] = 0;
            strcpy(plugin_path, PLUGIN_DIR);
            strcat(plugin_path, "/");
            strcat(plugin_path, dirent->d_name);
            plugin_load_file(plugin_path);
        }
    }

    closedir(dir);
}

static void set_pymodule_path(char * py_path)
{
    PyObject * sys_path = PySys_GetObject("path");        
    PyObject * path     = PyString_FromString(py_path);   
    PyList_Append(sys_path, path);
}

/*
 * Build an object to pass as a single parameter to the python plugin.
 */
static void py_call_module(struct py_module_t * mod, struct irc_t * irc, char * res)
{
    PyObject    * p_args;
    PyObject    * p_val;
    char        * t;

    p_args = PyTuple_New(2);                          

    p_val = PyString_FromString("");           
    PyTuple_SetItem(p_args, 0, p_val);            

    p_val = PyString_FromString(irc->message.trailing);        
    PyTuple_SetItem(p_args, 1, p_val);            

    p_val = PyObject_CallObject(mod->pFunc, p_args);  
    if (p_val) {
        t = PyString_AsString(p_val);                     
        strcpy(res, t);
    } else {
        sprintf(res, "PRIVMSG %s :Python module returned null.", irc->from);
    }
}

static struct plugin_t * plugin;

static void run(void)
{
    struct python_plugin_list_t * it;
    char command_name[50];

    command_name[0] = 0;

    if (!plugin->irc->message.trailing[0]) {
        size_t n = strcspn(plugin->irc->message.trailing+1, " \r\n");
        strncpy(command_name, plugin->irc->message.trailing+1, n);
        command_name[n] = 0;
    }

    for (it = head; it != NULL; it = it->next) {
        /* Run command. */
        if (it->cur->is_command && !strcmp(it->cur->name, command_name)) {
            struct py_module_t * module = it->cur;

            py_call_module(module, plugin->irc, plugin->irc->response);
        }

        /**
         * TODO: Run python loopers and greps.
         */
    }
}

static int manager_find (char * name) 
{
    struct python_plugin_list_t * it;

    for (it = head; it != NULL; it = it->next) {
        if (!strcmp(it->cur->name, name))
            return 0;
    }

    return -1;
}

static int init_python(void)
{
    char buf[200];
    char pwd[100];
    void * sym;
    void * handle;

    buf[0] = 0;
    pwd[0] = 0;

    if ( (handle = dlopen("python27.dll", RTLD_NOW|RTLD_GLOBAL)) == NULL
      && (handle = dlopen("python26.dll", RTLD_NOW|RTLD_GLOBAL)) == NULL
      && (handle = dlopen("libpython2.7.so", RTLD_NOW|RTLD_GLOBAL)) == NULL
      && (handle = dlopen("libpython2.6.so", RTLD_NOW|RTLD_GLOBAL)) == NULL
      && (handle = dlopen("python27.so", RTLD_NOW|RTLD_GLOBAL)) == NULL
      && (handle = dlopen("python26.so", RTLD_NOW|RTLD_GLOBAL)) == NULL) {
        fprintf(stderr, "%25s:%4d:Python shared library not found.\n", __FILE__, __LINE__);
        return -1;
    }
    fprintf(stderr, "%25s:%4d:Python found.\n", __FILE__, __LINE__);

    if ( (sym = dlsym(handle, "Py_Initialize")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: Py_Initialize\n", __FILE__, __LINE__);
        return -1;
    }
    Py_Initialize = sym;
   

    if ( (sym = dlsym(handle, "PyList_Append")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyList_Append\n", __FILE__, __LINE__);
        return -1;
    }
    PyList_Append = sym;

    if ( (sym = dlsym(handle, "PyString_FromString")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyString_FromString\n", __FILE__, __LINE__);
        return -1;
    }
    PyString_FromString = sym;

    if ( (sym = dlsym(handle, "PySys_GetObject")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PySys_GetObject\n", __FILE__, __LINE__);
        return -1;
    }
    PySys_GetObject = sym;

    if ( (sym = dlsym(handle, "PyObject_GetAttrString")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyObject_GetAttrString\n", __FILE__, __LINE__);
        return -1;
    }
    PyObject_GetAttrString = sym;

    if ( (sym = dlsym(handle, "PyImport_ImportModule")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyImport_ImportModule\n", __FILE__, __LINE__);
        return -1;
    }
    PyImport_ImportModule = sym;


    if ( (sym = dlsym(handle, "PyCallable_Check")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyCallable_Check\n", __FILE__, __LINE__);
        return -1;
    }
    PyCallable_Check = sym;

    if ( (sym = dlsym(handle, "PyTuple_New")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyTuple_New\n", __FILE__, __LINE__);
        return -1;
    }
    PyTuple_New = sym;

    if ( (sym = dlsym(handle, "PyObject_CallObject")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyObject_CallObject\n", __FILE__, __LINE__);
        return -1;
    }
    PyObject_CallObject = sym;

    if ( (sym = dlsym(handle, "PyString_AsString")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyString_AsString\n", __FILE__, __LINE__);
        return -1;
    }
    PyString_AsString = sym;

    if ( (sym = dlsym(handle, "PyTuple_SetItem")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyTuple_SetItem\n", __FILE__, __LINE__);
        return -1;
    }
    PyTuple_SetItem = sym;

    if ( (sym = dlsym(handle, "PyImport_AddModule")) == NULL) {
        fprintf(stderr, "%25s:%4d:Symbol not found: PyImport_AddModule\n", __FILE__, __LINE__);
        return -1;
    }
    PyImport_AddModule = sym;

    Py_Initialize();
    
    getcwd(pwd, 1024);
    sprintf(buf, "%s/%s/", pwd, PLUGIN_DIR);
    fprintf(stderr, "%25s:%4d:Setting python module path: %s\n", __FILE__, __LINE__, buf);
    set_pymodule_path(buf);

    return 0;
}

struct plugin_t * init(void)
{
    plugin = malloc(sizeof (struct plugin_t));
    memset(plugin, 0, sizeof *plugin);

    plugin->run        = run;
    plugin->name       = "python_loader";
    plugin->is_looper  = 1;
    plugin->is_command = 1;
    plugin->is_grep    = 1;
    plugin->is_manager = 1;

    plugin->manager_find = manager_find;

    if (init_python() != 0) {
        return NULL;    
    }
    load_python_plugins();

    return plugin;
}

int main(int argc, char *argv[])
{
    struct plugin_t * plugin;

    plugin = init();

    int n = manager_find("example");
    printf("%d\n", n);
    return 0;
}
