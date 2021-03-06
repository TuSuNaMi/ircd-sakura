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
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style channel mode +T */

class NoNotice : public ModeHandler
{
 public:
	NoNotice(InspIRCd* Instance) : ModeHandler(Instance, 'T', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('T'))
			{
				channel->SetMode('T',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('T'))
			{
				channel->SetMode('T',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoNotice : public Module
{
	
	NoNotice* nt;
 public:
 
	ModuleNoNotice(InspIRCd* Me)
		: Module(Me)
	{
		
		nt = new NoNotice(ServerInstance);
		if (!ServerInstance->AddMode(nt, 'T'))
			throw ModuleException("Could not add new modes!");
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNotice] = 1;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			chanrec* c = (chanrec*)dest;
			if (c->IsModeSet('T'))
			{
				if ((ServerInstance->ULine(user->server)) || (c->GetStatus(user) == STATUS_OP) || (c->GetStatus(user) == STATUS_HOP))
				{
					// ops and halfops can still /NOTICE the channel
					return 0;
				}
				else
				{
					user->WriteServ("404 %s %s :Can't send NOTICE to channel (+T set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleNoNotice()
	{
		ServerInstance->Modes->DelMode(nt);
		DELETE(nt);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleNoNotice)
