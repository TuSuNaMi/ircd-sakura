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
#include "hashcomp.h"
#include "wildcard.h"

/* $ModDesc: Provides support for the /SILENCE command */

/* Improved drop-in replacement for the /SILENCE command
 * syntax: /SILENCE [+|-]<mask> <p|c|i|n|t|a|x> as in <privatemessage|channelmessage|invites|privatenotice|channelnotice|all|exclude>
 *
 * example that blocks all except private messages
 *  /SILENCE +*!*@* a
 *  /SILENCE +*!*@* px
 *
 * example that blocks all invites except from channel services
 *  /SILENCE +*!*@* i
 *  /SILENCE +chanserv!services@chatters.net ix
 *
 * example that blocks some bad dude from private, notice and inviting you
 *  /SILENCE +*!kiddie@lamerz.net pin
 *
 * TODO: possibly have add and remove check for existing host and only modify flags according to
 *       what's been changed instead of having to remove first, then add if you want to change
 *       an entry.
 */

// pair of hostmask and flags
typedef std::pair<std::string, int> silenceset;

// deque list of pairs
typedef std::deque<silenceset> silencelist;

// intmasks for flags
static int SILENCE_PRIVATE	= 0x0001; /* p  private messages      */
static int SILENCE_CHANNEL	= 0x0002; /* c  channel messages      */
static int SILENCE_INVITE	= 0x0004; /* i  invites               */
static int SILENCE_NOTICE	= 0x0008; /* n  notices               */
static int SILENCE_CNOTICE	= 0x0010; /* t  channel notices       */
static int SILENCE_ALL		= 0x0020; /* a  all, (pcint)          */
static int SILENCE_EXCLUDE	= 0x0040; /* x  exclude this pattern  */


class cmd_silence : public command_t
{
	unsigned int& maxsilence;
 public:
	cmd_silence (InspIRCd* Instance, unsigned int &max) : command_t(Instance,"SILENCE", 0, 0), maxsilence(max)
	{
		this->source = "m_silence_ext.so";
		syntax = "{[+|-]<mask> <p|c|i|n|t|a|x>}";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (!pcnt)
		{
			// no parameters, show the current silence list.
			// Use Extensible::GetExt to fetch the silence list
			silencelist* sl;
			user->GetExt("silence_list", sl);
			// if the user has a silence list associated with their user record, show it
			if (sl)
			{
				for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
				{
					user->WriteServ("271 %s %s %s %s",user->nick, user->nick,c->first.c_str(), DecompPattern(c->second).c_str());
				}
			}
			user->WriteServ("272 %s :End of Silence List",user->nick);

			return CMD_LOCALONLY;
		}
		else if (pcnt > 0)
		{
			// one or more parameters, add or delete entry from the list (only the first parameter is used)
			std::string mask = parameters[0] + 1;
			char action = *parameters[0];
			// Default is private and notice so clients do not break
			int pattern = CompilePattern("pn");

			// if pattern supplied, use it
			if (pcnt > 1) {
				pattern = CompilePattern(parameters[1]);
			}
			
			if (!mask.length())
			{
				// 'SILENCE +' or 'SILENCE -', assume *!*@*
				mask = "*!*@*";
			}
			
			ModeParser::CleanMask(mask);

			if (action == '-')
			{
				// fetch their silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// does it contain any entries and does it exist?
				if (sl)
				{
					for (silencelist::iterator i = sl->begin(); i != sl->end(); i++)
					{
						// search through for the item
						irc::string listitem = i->first.c_str();
						if (listitem == mask && i->second == pattern)
						{
							sl->erase(i);
							user->WriteServ("950 %s %s :Removed %s %s from silence list",user->nick, user->nick, mask.c_str(), DecompPattern(pattern).c_str());
							if (!sl->size())
							{
								DELETE(sl);
								user->Shrink("silence_list");
							}
							break;
						}
					}
				}
				user->WriteServ("952 %s %s :%s %s does not exist on your silence list",user->nick, user->nick, mask.c_str(), DecompPattern(pattern).c_str());
			}
			else if (action == '+')
			{
				// fetch the user's current silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// what, they dont have one??? WE'RE ALL GONNA DIE! ...no, we just create an empty one.
				if (!sl)
				{
					sl = new silencelist;
					user->Extend("silence_list", sl);
				}
				if (sl->size() > maxsilence)
				{
					user->WriteServ("952 %s %s :Your silence list is full",user->nick, user->nick);
					return CMD_FAILURE;
				}
				for (silencelist::iterator n = sl->begin(); n != sl->end();  n++)
				{
					irc::string listitem = n->first.c_str();
					if (listitem == mask && n->second == pattern)
					{
						user->WriteServ("952 %s %s :%s %s is already on your silence list",user->nick, user->nick, mask.c_str(), DecompPattern(pattern).c_str());
						return CMD_FAILURE;
					}
				}
				if (((pattern & SILENCE_EXCLUDE) > 0))
				{
					sl->push_front(silenceset(mask,pattern));
				}
				else
				{
					sl->push_back(silenceset(mask,pattern));
				}
				user->WriteServ("951 %s %s :Added %s %s to silence list",user->nick, user->nick, mask.c_str(), DecompPattern(pattern).c_str());
				return CMD_LOCALONLY;
			}
		}
		return CMD_LOCALONLY;
	}

	/* turn the nice human readable pattern into a mask */
	int CompilePattern(const char* pattern)
	{
		int p = 0;
		for (const char* n = pattern; *n; n++)
		{
			switch (*n)
			{
				case 'p':
					p |= SILENCE_PRIVATE;
					break;
				case 'c':
					p |= SILENCE_CHANNEL;
					break;
				case 'i': 
					p |= SILENCE_INVITE;
					break;
				case 'n':
					p |= SILENCE_NOTICE;
					break;
				case 't':
					p |= SILENCE_CNOTICE;
					break;
				case 'a':
					p |= SILENCE_ALL;
					break;
				case 'x':
					p |= SILENCE_EXCLUDE;
					break;
				default:
					break;
			}
		}
		return p;
	}

	/* turn the mask into a nice human readable format */
	std::string DecompPattern (const int pattern)
	{
		std::string out;
		if ((pattern & SILENCE_PRIVATE) > 0)
			out += ",privatemessages";
		if ((pattern & SILENCE_CHANNEL) > 0)
			out += ",channelmessages";
		if ((pattern & SILENCE_INVITE) > 0)
			out += ",invites";
		if ((pattern & SILENCE_NOTICE) > 0)
			out += ",privatenotices";
		if ((pattern & SILENCE_CNOTICE) > 0)
			out += ",channelnotices";
		if ((pattern & SILENCE_ALL) > 0)
			out = ",all";
		if ((pattern & SILENCE_EXCLUDE) > 0)
			out += ",exclude";
		return "<" + out.substr(1) + ">";
	}

};

class ModuleSilence : public Module
{
	cmd_silence* mycommand;
	unsigned int maxsilence;
 public:
 
	ModuleSilence(InspIRCd* Me)
		: Module(Me), maxsilence(32)
	{
		OnRehash(NULL, "");
		mycommand = new cmd_silence(ServerInstance,maxsilence);
		ServerInstance->AddCommand(mycommand);
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		maxsilence = Conf.ReadInteger("silence", "maxentries", 0, true);
		if (!maxsilence)
			maxsilence = 32;
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnBuildExemptList] = List[I_OnUserQuit] = List[I_On005Numeric] = List[I_OnUserPreNotice] = List[I_OnUserPreMessage] = List[I_OnUserPreInvite] = 1;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message)
	{
		// when the user quits tidy up any silence list they might have just to keep things tidy
		silencelist* sl;
		user->GetExt("silence_list", sl);
		if (sl)
		{
			DELETE(sl);
			user->Shrink("silence_list");
		}
	}

	virtual void On005Numeric(std::string &output)
	{
		// we don't really have a limit...
		output = output + " ESILENCE SILENCE=" + ConvToStr(maxsilence);
	}

	virtual void OnBuildExemptList(MessageType message_type, chanrec* chan, userrec* sender, char status, CUList &exempt_list)
	{
		int public_silence = (message_type == MSG_PRIVMSG ? SILENCE_CHANNEL : SILENCE_CNOTICE);
		CUList *ulist;
		switch (status)
		{
			case '@':
				ulist = chan->GetOppedUsers();
				break;
			case '%':
				ulist = chan->GetHalfoppedUsers();
				break;
			case '+':
				ulist = chan->GetVoicedUsers();
				break;
			default:
				ulist = chan->GetUsers();
				break;
		}

		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			if (IS_LOCAL(i->first))
			{
				if (MatchPattern(i->first, sender, public_silence) == 1)
				{
					exempt_list[i->first] = i->first->nick;
				}
			}
		}
	}

	virtual int PreText(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list, int silence_type)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_USER)
		{
			return MatchPattern((userrec*)dest, user, silence_type);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			chanrec* chan = (chanrec*)dest;
			if (chan)
			{
				this->OnBuildExemptList((silence_type == SILENCE_PRIVATE ? MSG_PRIVMSG : MSG_NOTICE), chan, user, status, exempt_list);
			}
		}
		return 0;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return PreText(user, dest, target_type, text, status, exempt_list, SILENCE_PRIVATE);
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return PreText(user, dest, target_type, text, status, exempt_list, SILENCE_NOTICE);
	}

	virtual int OnUserPreInvite(userrec* source,userrec* dest,chanrec* channel)
	{
		return MatchPattern(dest, source, SILENCE_INVITE);
	}

	int MatchPattern(userrec* dest, userrec* source, int pattern)
	{
		silencelist* sl;
		dest->GetExt("silence_list", sl);
		if (sl)
		{
			for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
			{
				if (((((c->second & pattern) > 0)) || ((c->second & SILENCE_ALL) > 0)) && (ServerInstance->MatchText(source->GetFullHost(), c->first)))
					return !(((c->second & SILENCE_EXCLUDE) > 0));
			}
		}
		return 0;
	}

	virtual ~ModuleSilence()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleSilence)
