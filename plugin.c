#include "plugin.h"
#include "plugins/plugin_client.h"

#include "global.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dirent.h>
#include <dlfcn.h>

#define PLUGIN_DIR "plugins"

struct plugin_list_t * plugin_list_head;

/**
 * Returns a handle to the native plugin, for the given command name.
 */
struct plugin_t ** plugin_find_commands(char * name, struct plugin_t ** plugin_list)
{
    struct plugin_list_t * it;
    int i = 0;

    for (it = plugin_list_head; it != NULL; it = it->next) {
        if (!it->cur->is_command)
            continue;

        if (it->cur->is_manager && !it->cur->manager_find(name))
            plugin_list[i++] = it->cur;

        if (!strcmp(it->cur->name, name))
            plugin_list[i++] = it->cur;
    }

    plugin_list[i] = NULL;
    return plugin_list;
}

void plugin_insert(struct plugin_t * p)
{
    if (plugin_list_head == NULL) {
        plugin_list_head = malloc(sizeof (struct plugin_list_t));
        plugin_list_head->cur = p;
        plugin_list_head->next = NULL;
    } else {
        struct plugin_list_t * it;

        for (it = plugin_list_head; it->next != NULL; it = it->next) {}

        it->next = malloc(sizeof (struct plugin_list_t));
        it->next->cur = p;
        it->next->next = NULL;
    }
}

/**
 * Callback send raw message function for plugins.
 *
 * TODO: It should utilize a message queue for delivering messages to the network.
 *
 */
void send_message(struct irc_t * irc)
{
    debug(irc->response);
    socket_send_message(&irc->session->socket, irc->response);
}

struct plugin_t plugin_list[100];

void plugin_load_file(char * file)
{
    void * plugin_file;
    struct plugin_t * (*init)(void);
    struct plugin_t * plugin;

    debug("Loading native plugin \"%s\"\n", file);
    if ((plugin_file = dlopen(file, RTLD_LAZY)) == NULL) {
        debug("The plugin file \"%s\" could not be opened.\n", file);
        return;
    }

    if ((init = dlsym(plugin_file, "init")) == NULL) { 
        debug("The plugin \"%s\" has no exported init function symbol.\n", file);
    }

    /* initialize and get handle to the plugin */
    if ((plugin = init()) == NULL) {
        debug("The plugin initialization is failed for reasons unknown."
                " Hope it's last words were meaningful, if any.\n");
        return;
    }

    /* See warning message for commentary */
    if (!plugin->is_manager && (plugin->is_command + plugin->is_grep + plugin->is_looper) > 1) {
        debug("The plugin \"%s\" is not valid."
                    "A plugin can only be one of type `command', `grep' and `looper'.\n",
                    file);
        /* TODO: Clean up. */
        return;
    }

    /* Attach callbacks to be used by plugin. */
    plugin->send_message = send_message;

    /* If plugin is of type grep, acquire grep keywords. */
    if (!plugin->is_manager && plugin->is_grep) {
        char ** keywords;

        if ((keywords = dlsym(plugin_file, "keywords"))) {
            plugin->keywords = keywords;
        } else { 
            debug("The plugin \"%s\" is of type `grep', but has no "
                        "exported grep keywords symbols found. Discarding.\n", file);
            /* TODO: Clean up. */
            return;
        }
    }

    plugin_insert(plugin);
}

void plugin_init()
{
    DIR * dir;
    struct dirent * dirent;

    dir = opendir(PLUGIN_DIR);

	if (!dir)
	{
		debug("no modules found, skipping.\n");
		return;
	}
    
    while ((dirent = readdir(dir)) != NULL)
    {
        if (strstr(dirent->d_name, ".so")) {
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


#if TEST_PLUGIN
int main()
{
    struct plugin_list_t * it;

    plugin_init();

    puts("Iterating the plugins...");
    for (it = plugin_list_head; it != NULL; it = it->next) {
        printf("- %s\n", it->cur->name);
    }
    puts("Finished.");

    return 0;
}
#endif

