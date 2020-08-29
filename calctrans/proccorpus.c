/*
 * コーパスとなる文章を読んで、文節の長さを調整して
 * 形態素解析の結果を出力する
 *
 * 出力形式について
 *  まず伸縮を行った文節が最初の長さで出力される
 *  次に各文節毎に(あれば)誤った候補、正しい候補の順で情報を出力する
 *
 *
 * Copyright (C) 2006-2007 TABATA Yusuke
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <anthy/convdb.h>

static int verbose;

/* 文節の長さを例文にあわせる */
static int
trim_segment(anthy_context_t ac, struct conv_res *cr,
	     int nth, char *seg)
{
  int len = strlen(seg);
  int resized = 0;
  int cur_len = -1; /* dummy initial value */
  int old_len;
  (void)cr;

  while (1) {
    char seg_buf[1024];

    /* "~" may be returned to cause infinite loop when corpus has "～" */
    anthy_get_segment(ac, nth, NTH_UNCONVERTED_CANDIDATE, seg_buf, 1024);
    old_len = cur_len;
    cur_len = strlen(seg_buf);
    if (len == cur_len) {
      return 1;
    }
    if (cur_len == old_len) {
      /* exit for infinite loop situation  */
      fprintf(stderr, "\n*** Ignore an infinite loop situation ***\n");
      fprintf(stderr, ">>> src=(%s) -> cand=(%s), seg=(%s) seg_buf=(%s)\n\n",
          cr->src_str, cr->cand_str, seg, seg_buf);
      return 0;
    }
    if (!resized) {
      resized = 1;
      /* 伸縮前の文節の情報を表示する */
      print_size_miss_segment_info(ac, nth);
    }
    if (len > cur_len) {
      anthy_resize_segment(ac, nth, 1);
    } else {
      anthy_resize_segment(ac, nth, -1);
    }
  }
  return 0;
}

/*
 * nth番目の文節で候補segを探して確定する
 */
static int
find_candidate(anthy_context_t ac, struct conv_res *cr,
	       int nth, char *seg)
{
  char seg_buf[1024];
  int i;
  struct anthy_segment_stat ass;

  if (seg[0] == '~') {
    /* 候補ミスのマーク「~」をスキップする */
    seg++;
    cr->cand_check[nth] = 1;
  }

  anthy_get_segment_stat(ac, nth, &ass);
  for (i = 0; i < ass.nr_candidate; i++) {
    anthy_get_segment(ac, nth, i, seg_buf, 1024);
    if (!strcmp(seg_buf, seg)) {
      /* 一致する候補を見つけたので確定する */
      anthy_commit_segment(ac, nth, i);
      return 0;
    }
  }
  return 0;
}

/* '|' で文節に区切られた文字列の各文節を引数にfnを呼ぶ */
static int
for_each_segment(anthy_context_t ac, struct conv_res *cr,
		 const char *res_str,
		 int (*fn)(anthy_context_t ac, struct conv_res *cr,
			   int nth, char *seg))
{
  char *str, *cur, *cur_seg;
  int nth;
  if (!res_str) {
    return 0;
  }

  str = strdup(res_str);
  cur = str;
  cur ++;
  cur_seg = cur;
  nth = 0;
  while ((cur = strchr(cur, '|'))) {
    *cur = 0;
    /**/
    if (fn) {
      fn(ac, cr, nth, cur_seg);
    }
    /**/
    nth ++;
    cur ++;
    cur_seg = cur;
  }

  free(str);
  
  return 1;
}

static void
proc_sentence(anthy_context_t ac, struct conv_res *cr)
{
  int i;
  struct anthy_conv_stat acs;
  /*printf("(%s)\n", cr->src_str);*/
  anthy_set_string(ac, cr->src_str);
  /* 文節の長さを調節する */
  if (!for_each_segment(ac, cr, cr->res_str, trim_segment)) {
    return ;
  }
  /**/
  if (anthy_get_stat(ac, &acs)) {
    return ;
  }
  cr->cand_check = malloc(sizeof(int) * acs.nr_segment);
  for (i = 0; i < acs.nr_segment; i++) {
    cr->cand_check[i] = 0;
  }

  /* 候補を選択する */
  if (cr->cand_str) {
    for_each_segment(ac, cr, cr->cand_str, find_candidate);
  }

  if (verbose) {
    anthy_print_context(ac);
  }
  /* 出力する */
  print_context_info(ac, cr);
}

int
main(int argc, char **argv)
{
  struct res_db *db;
  struct conv_res *cr;
  anthy_context_t ac;
  int i;
  const char *dic = "anthy.dic";

  db = create_db();
  for (i = 1; i < argc; i++)
    if (!strcmp("-d", argv[i]) && (i+1) < argc)
      dic = argv[++i];
    else if (!strcmp("-v", argv[i]))
      verbose = 1;
    else
      read_db(db, argv[i]);

  anthy_conf_override("CONFFILE", "../anthy-test-conf");
  anthy_conf_override("DIC_FILE", dic);
  anthy_init();
  anthy_set_personality("");
  anthy_xstr_set_print_encoding (ANTHY_UTF8_ENCODING);
  ac = anthy_create_context();

  /**/
  for (cr = db->res_list.next; cr; cr = cr->next) {
    proc_sentence(ac, cr);
  }
  return 0;
}
