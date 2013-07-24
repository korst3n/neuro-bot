CFLAGS = -Wall -g -O0

ifneq ($(OS), Windows_NT)
	CFLAGS		 	+= -fPIC
endif

all: plugin_python_manager.so plugin_git.so plugin_title.so plugin_wiki.so plugin_youtube.so plugin_test.so

plugin_python_manager.so: plugin_python_manager.c
	$(CC) plugin_python_manager.c $(CFLAGS) --shared -o$@

plugin_git.so: plugin_git.c
	$(CC) plugin_git.c utils/curl_wrapper.c utils/json.c $(CFLAGS) `pkg-config libcurl --cflags --libs` --shared -o$@

plugin_title.so: plugin_title.c
	$(CC) plugin_title.c utils/curl_wrapper.c $(CFLAGS) `pkg-config libcurl libxml-2.0 --cflags --libs` --shared -o$@

plugin_wiki.so: plugin_wiki.c
	$(CC) plugin_wiki.c utils/curl_wrapper.c utils/json.c $(CFLAGS) `pkg-config libcurl --cflags --libs` --shared -o$@

plugin_youtube.so: plugin_youtube.c
	$(CC) plugin_youtube.c utils/curl_wrapper.c utils/json.c $(CFLAGS) `pkg-config libcurl --cflags --libs` --shared -o$@

plugin_test.so: plugin_test.c
	$(CC) plugin_test.c $(CFLAGS) --shared -o$@


clean:
	rm -fv *.o *.so
