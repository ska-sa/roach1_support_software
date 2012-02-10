/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

//#include "cmd_roachtest.c"

#include <common.h>
#include <command.h>
#include <malloc.h>

#if (CONFIG_COMMANDS & CFG_CMD_ROACH)

#define STATUS_NAME (64 - 8)

struct status_entry{
        char s_name[STATUS_NAME];
        unsigned int s_code;
        struct status_entry *s_next;
};

static struct status_entry *status_top = NULL;

#if 0
int do_pattern(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
        ulong begin, end;

        v = 0;

        if (argc < 3) {
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
        }

        begin = simple_strtoul(argv[1], NULL, 16);

}
#endif

int do_check(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
        cmd_tbl_t *newtp;
        struct status_entry *s;

        if (argc < 3) {
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
        }

        s = malloc(sizeof(struct status_entry));
        if(s == NULL){
        	return 1;
        }

        strncpy(s->s_name, argv[1], STATUS_NAME - 1);
        s->s_name[STATUS_NAME - 1] = '\0';
        s->s_code = 1;
        s->s_next = NULL;

        if(status_top == NULL){
        	status_top = s;
        } else {
                struct status_entry *sp;
        	for(sp = status_top; sp->s_next; sp = sp->s_next);
        	sp->s_next = s;
        }

        /* Look up command in command table */
        if ((newtp = find_cmd(argv[2])) == NULL) {
                printf ("Unknown command '%s' - try 'help'\n", argv[0]);
                return 1;
        }

        /* OK - call function to do the command */
        if ((newtp->cmd) (newtp, flag, argc - 2, &(argv[2])) == 0) {
                s->s_code = 0;
        }

        return s->s_code;
}

int do_report(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
        struct status_entry *s;
        unsigned int total, fails;

        if(argc > 1){
        	while(status_top){
        		s = status_top;
        		status_top = status_top->s_next;
        		free(s);
                }
                printf("clearing report log\n");
        } else {
                total = 0;
                fails = 0;
                for(s = status_top; s; s = s->s_next){
                        printf("%s:\t%s\n", s->s_name, s->s_code ? "failed" : "ok");
                        if(s->s_code){
                                fails++;
                        }
                        total++;
                }
                printf("%u checks, %u %s\n", total, fails, (fails == 1) ? "failure" : "failures");
        }


        return 0;
}

/***************************************************/

U_BOOT_CMD(
	check,	CFG_MAXARGS,	1,	do_check,
	"check   - run a command, recording its status\n",
	"label command [args] - run a command, recording its status\n"
);

U_BOOT_CMD(
	report,	2,	1,	do_report,
	"report  - print a list of checks\n",
	"print previous check outcomes and optionally clear the list\n"
);

#endif /* CFG_CMD_ROACH */
