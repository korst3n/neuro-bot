#include "neurobotapi.h"

#include <string.h>
#include <ctype.h>

char *  (*curl_perform)(char * url);
void    (*n_strip_tags)(char * dest, char * src);
char *  (*n_get_tag_value)(char * body, char * tagname);

void init(void ** fp_list)
{
    curl_perform    = fp_list[0];
    n_strip_tags    = fp_list[1];
    n_get_tag_value = fp_list[2];
}

