#include <stdio.h>
#include <katcp.h>

#include "multiserver.h"

int x_issue_help(struct mul_client *ci, struct mul_msg *mm);

struct xlookup_entry x_test_table[] = {
  { "foobar", &x_issue_help, NULL,                     0, 1,            XLK_LABEL_FAIL },
  { "foobar", &x_issue_help, NULL,                     0, 2,            XLK_LABEL_FAIL },
  { NULL,     NULL,          &generic_collect_xlookup, 0, XLK_LABEL_OK, XLK_LABEL_FAIL },
  { NULL,     NULL,          NULL, 0, 0, 0 }
};

int x_issue_help(struct mul_client *ci, struct mul_msg *mm)
{
#ifdef DEBUG
  fprintf(stderr, "x: about to issue help message\n");
#endif
  if(append_string_msg(mm, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, "?help") < 0){
    return -1;
  }

  return 1;
}

int x_test_cmd(struct katcp_dispatch *d, int argc)
{
  return enter_client_xlookup(d, x_test_table);
}

