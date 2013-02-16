﻿#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "session.h"
#include "channel.h"
#include "user.h"


int main(int argc, char *argv[])
{
	struct session_t * session = session_create();

	session_channel_add(session, "#archlinux-tr");
	session_channel_add(session, "#literature");

	channel_user_add(session_channel_find_by_name(session, "#archlinux-tr"), user_create("neuro_sys"));
	channel_user_add(session_channel_find_by_name(session, "#archlinux-tr"), user_create("mrcan"));
	channel_user_add(session_channel_find_by_name(session, "#archlinux-tr"), user_create("emrahnzm"));
	channel_user_add(session_channel_find_by_name(session, "#archlinux-tr"), user_create("cmdexe"));
	channel_user_add(session_channel_find_by_name(session, "#archlinux-tr"), user_create("rasir"));

	session_channel_print(session);

	session_destroy(session);

	return 0;
}
