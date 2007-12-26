/*
 * Unit test for util.h
 * 
 * This test utiliy functions
 */
#include "test.h"
#include "util.h"

#include <string.h>

static void test_div_up()
{
    CU_ASSERT_EQUAL( div_up(1, 2), 1 );
    CU_ASSERT_EQUAL( div_up(2, 2), 1 );
    CU_ASSERT_EQUAL( div_up(0, 2), 0 );
    CU_ASSERT_EQUAL( div_up(-1, 2), 0 );
    CU_ASSERT_EQUAL( div_up(3, 2), 2 );
}

static void test_round_up()
{
    CU_ASSERT_EQUAL( round_up(1, 2), 2 );
    CU_ASSERT_EQUAL( round_up(2, 2), 2 );
    CU_ASSERT_EQUAL( round_up(0, 2), 0 );
    CU_ASSERT_EQUAL( round_up(-1, 2), 0 );
    CU_ASSERT_EQUAL( round_up(3, 2), 4 );
    CU_ASSERT_EQUAL( round_up(15, 7), 21 );
    CU_ASSERT_EQUAL( round_up(13, 7), 14 );
    CU_ASSERT_EQUAL( round_up(14, 7), 14 );
}

static void test_iso_lsb_msb()
{
    uint8_t buf[4];
    uint32_t num;

    num = 0x01020304;
    iso_lsb(buf, num, 4);
    CU_ASSERT_EQUAL( buf[0], 0x04 );
    CU_ASSERT_EQUAL( buf[1], 0x03 );
    CU_ASSERT_EQUAL( buf[2], 0x02 );
    CU_ASSERT_EQUAL( buf[3], 0x01 );

    iso_msb(buf, num, 4);
    CU_ASSERT_EQUAL( buf[0], 0x01 );
    CU_ASSERT_EQUAL( buf[1], 0x02 );
    CU_ASSERT_EQUAL( buf[2], 0x03 );
    CU_ASSERT_EQUAL( buf[3], 0x04 );

    iso_lsb(buf, num, 2);
    CU_ASSERT_EQUAL( buf[0], 0x04 );
    CU_ASSERT_EQUAL( buf[1], 0x03 );

    iso_msb(buf, num, 2);
    CU_ASSERT_EQUAL( buf[0], 0x03 );
    CU_ASSERT_EQUAL( buf[1], 0x04 );
}

static void test_iso_read_lsb_msb()
{
    uint8_t buf[4];
    uint32_t num;

    buf[0] = 0x04;
    buf[1] = 0x03;
    buf[2] = 0x02;
    buf[3] = 0x01;

    num = iso_read_lsb(buf, 4);
    CU_ASSERT_EQUAL(num, 0x01020304);

    num = iso_read_msb(buf, 4);
    CU_ASSERT_EQUAL(num, 0x04030201);

    num = iso_read_lsb(buf, 2);
    CU_ASSERT_EQUAL(num, 0x0304);

    num = iso_read_msb(buf, 2);
    CU_ASSERT_EQUAL(num, 0x0403);
}

static void test_iso_bb()
{
    uint8_t buf[8];
    uint32_t num;

    num = 0x01020304;
    iso_bb(buf, num, 4);
    CU_ASSERT_EQUAL( buf[0], 0x04 );
    CU_ASSERT_EQUAL( buf[1], 0x03 );
    CU_ASSERT_EQUAL( buf[2], 0x02 );
    CU_ASSERT_EQUAL( buf[3], 0x01 );
    CU_ASSERT_EQUAL( buf[4], 0x01 );
    CU_ASSERT_EQUAL( buf[5], 0x02 );
    CU_ASSERT_EQUAL( buf[6], 0x03 );
    CU_ASSERT_EQUAL( buf[7], 0x04 );

    iso_bb(buf, num, 2);
    CU_ASSERT_EQUAL( buf[0], 0x04 );
    CU_ASSERT_EQUAL( buf[1], 0x03 );
    CU_ASSERT_EQUAL( buf[2], 0x03 );
    CU_ASSERT_EQUAL( buf[3], 0x04 );
}

static void test_iso_1_dirid()
{
    CU_ASSERT_STRING_EQUAL( iso_1_dirid("dir1"), "DIR1" );
    CU_ASSERT_STRING_EQUAL( iso_1_dirid("dIR1"), "DIR1" );
    CU_ASSERT_STRING_EQUAL( iso_1_dirid("DIR1"), "DIR1" );
    CU_ASSERT_STRING_EQUAL( iso_1_dirid("dirwithbigname"), "DIRWITHB");
    CU_ASSERT_STRING_EQUAL( iso_1_dirid("dirwith8"), "DIRWITH8");
    CU_ASSERT_STRING_EQUAL( iso_1_dirid("dir.1"), "DIR_1");
    CU_ASSERT_STRING_EQUAL( iso_1_dirid("4f<0KmM::xcvf"), "4F_0KMM_");
}

static void test_iso_2_dirid()
{
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("dir1"), "DIR1" );
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("dIR1"), "DIR1" );
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("DIR1"), "DIR1" );
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("dirwithbigname"), "DIRWITHBIGNAME");
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("dirwith8"), "DIRWITH8");
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("dir.1"), "DIR_1");
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("4f<0KmM::xcvf"), "4F_0KMM__XCVF");
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("directory with 31 characters ok"), "DIRECTORY_WITH_31_CHARACTERS_OK");
    CU_ASSERT_STRING_EQUAL( iso_2_dirid("directory with more than 31 characters"), "DIRECTORY_WITH_MORE_THAN_31_CHA");
}

static void test_iso_1_fileid()
{
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file1"), "FILE1.");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("fILe1"), "FILE1.");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("FILE1"), "FILE1.");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid(".EXT"), ".EXT");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file.ext"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("fiLE.ext"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file.EXt"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("FILE.EXT"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("bigfilename"), "BIGFILEN.");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("bigfilename.ext"), "BIGFILEN.EXT");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("bigfilename.e"), "BIGFILEN.E");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file.bigext"), "FILE.BIG");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid(".bigext"), ".BIG");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("bigfilename.bigext"), "BIGFILEN.BIG");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file<:a.ext"), "FILE__A.EXT");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file.<:a"), "FILE.__A");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file<:a.--a"), "FILE__A.__A");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file.ex1.ex2"), "FILE_EX1.EX2");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("file.ex1.ex2.ex3"), "FILE_EX1.EX3");
    CU_ASSERT_STRING_EQUAL( iso_1_fileid("fil.ex1.ex2.ex3"), "FIL_EX1_.EX3");
}

static void test_iso_2_fileid()
{
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file1"), "FILE1.");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("fILe1"), "FILE1.");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("FILE1"), "FILE1.");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid(".EXT"), ".EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file.ext"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("fiLE.ext"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file.EXt"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("FILE.EXT"), "FILE.EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("bigfilename"), "BIGFILENAME.");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("bigfilename.ext"), "BIGFILENAME.EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("bigfilename.e"), "BIGFILENAME.E");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("31 characters filename.extensio"), "31_CHARACTERS_FILENAME.EXTENSIO");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("32 characters filename.extension"), "32_CHARACTERS_FILENAME.EXTENSIO");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("more than 30 characters filename.extension"), "MORE_THAN_30_CHARACTERS_FIL.EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file.bigext"), "FILE.BIGEXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid(".bigext"), ".BIGEXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("bigfilename.bigext"), "BIGFILENAME.BIGEXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file<:a.ext"), "FILE__A.EXT");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file.<:a"), "FILE.__A");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file<:a.--a"), "FILE__A.__A");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file.ex1.ex2"), "FILE_EX1.EX2");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("file.ex1.ex2.ex3"), "FILE_EX1_EX2.EX3");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid("fil.ex1.ex2.ex3"), "FIL_EX1_EX2.EX3");
    CU_ASSERT_STRING_EQUAL( iso_2_fileid(".file.bigext"), "_FILE.BIGEXT");
}

static void test_iso_rbtree_insert()
{
    int res;
    IsoRBTree *tree;
    char *str1, *str2, *str3, *str4, *str5;
    void *str;

    res = iso_rbtree_new(strcmp, &tree);
    CU_ASSERT_EQUAL(res, 1);

    /* ok, insert one str  */
    str1 = "first str";
    res = iso_rbtree_insert(tree, str1, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str1);

    str2 = "second str";
    res = iso_rbtree_insert(tree, str2, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str2);

    /* an already inserted string */
    str3 = "second str";
    res = iso_rbtree_insert(tree, str3, &str);
    CU_ASSERT_EQUAL(res, 0);
    CU_ASSERT_PTR_EQUAL(str, str2);

    /* an already inserted string */
    str3 = "first str";
    res = iso_rbtree_insert(tree, str3, &str);
    CU_ASSERT_EQUAL(res, 0);
    CU_ASSERT_PTR_EQUAL(str, str1);

    str4 = "a string to be inserted first";
    res = iso_rbtree_insert(tree, str4, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str4);

    str5 = "this to be inserted last";
    res = iso_rbtree_insert(tree, str5, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str5);

    /*
     * TODO write a really good test to check all possible estrange
     * behaviors of a red-black tree
     */

    iso_rbtree_destroy(tree, NULL);
}

void add_util_suite()
{
    CU_pSuite pSuite = CU_add_suite("UtilSuite", NULL, NULL);

    CU_add_test(pSuite, "div_up()", test_div_up);
    CU_add_test(pSuite, "round_up()", test_round_up);
    CU_add_test(pSuite, "iso_bb()", test_iso_bb);
    CU_add_test(pSuite, "iso_lsb/msb()", test_iso_lsb_msb);
    CU_add_test(pSuite, "iso_read_lsb/msb()", test_iso_read_lsb_msb);
    CU_add_test(pSuite, "iso_1_dirid()", test_iso_1_dirid);
    CU_add_test(pSuite, "iso_2_dirid()", test_iso_2_dirid);
    CU_add_test(pSuite, "iso_1_fileid()", test_iso_1_fileid);
    CU_add_test(pSuite, "iso_2_fileid()", test_iso_2_fileid);
    CU_add_test(pSuite, "iso_rbtree_insert()", test_iso_rbtree_insert);
}
