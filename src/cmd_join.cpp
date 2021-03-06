/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "commands/cmd_join.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_join(Instance);
}

/** Handle /JOIN
 */
CmdResult cmd_join::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt > 1)
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0, 1))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0]))
		{
			chanrec::JoinUser(ServerInstance, user, parameters[0], false, parameters[1]);
			return CMD_SUCCESS;
		}
	}
	else
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0]))
		{
			chanrec::JoinUser(ServerInstance, user, parameters[0], false, "");
			return CMD_SUCCESS;
		}
	}

	user->WriteServ("403 %s %s :Invalid channel name",user->nick, parameters[0]);
	return CMD_FAILURE;
}
