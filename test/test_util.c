/*
 * Unit test for util.h
 * 
 * This test utiliy functions
 */
#include "test.h"
#include "util.h"
#include "error.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

static void test_strconv()
{
    int ret;
    char *out;
    
    /* Prova de cadeia com codificação ISO-8859-15 */
    unsigned char in1[45] = 
         {0x50, 0x72, 0x6f, 0x76, 0x61, 0x20, 0x64, 0x65, 0x20, 0x63, 0x61, 
          0x64, 0x65, 0x69, 0x61, 0x20, 0x63, 0x6f, 0x6d, 0x20, 0x63, 0x6f, 
          0x64, 0x69, 0x66, 0x69, 0x63, 0x61, 0xe7, 0xe3, 0x6f, 0x20, 0x49, 
          0x53, 0x4f, 0x2d, 0x38, 0x38, 0x35, 0x39, 0x2d, 0x31, 0x35, 0x0a, 
          0x00}; /* encoded in ISO-8859-15 */
    unsigned char out1[47] = 
         {0x50, 0x72, 0x6f, 0x76, 0x61, 0x20, 0x64, 0x65, 0x20, 0x63, 0x61, 
          0x64, 0x65, 0x69, 0x61, 0x20, 0x63, 0x6f, 0x6d, 0x20, 0x63, 0x6f, 
          0x64, 0x69, 0x66, 0x69, 0x63, 0x61, 0xc3, 0xa7, 0xc3, 0xa3, 0x6f, 
          0x20, 0x49, 0x53, 0x4f, 0x2d, 0x38, 0x38, 0x35, 0x39, 0x2d, 0x31, 
          0x35, 0x0a, 0x00}; /* encoded in UTF-8 */
    unsigned char in2[45] = 
         {0x50, 0x72, 0x6f, 0x76, 0x61, 0x20, 0x64, 0x65, 0x20, 0x63, 0x61, 
          0x64, 0x65, 0x69, 0x61, 0x20, 0x63, 0x6f, 0x6d, 0x20, 0x63, 0x6f, 
          0x64, 0x69, 0x66, 0x69, 0x63, 0x61, 0xe7, 0xe3, 0x6f, 0x20, 0x49, 
          0x53, 0x4f, 0x2d, 0x38, 0x38, 0xff, 0xff, 0x2d, 0x31, 0x35, 0x0a, 
          0x00}; /* incorrect encoding */
    
    /* ISO-8859-15 to UTF-8 */
    ret = strconv((char*)in1, "ISO-8859-15", "UTF-8", &out);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_STRING_EQUAL(out, (char*)out1);
    free(out);
    
    /* UTF-8 to ISO-8859-15 */
    ret = strconv((char*)out1, "UTF-8", "ISO-8859-15", &out);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_STRING_EQUAL(out, (char*)in1);
    free(out);
    
    /* try with an incorrect input */
    ret = strconv((char*)in2, "UTF-8", "ISO-8859-15", &out);
    CU_ASSERT_EQUAL(ret, ISO_CHARSET_CONV_ERROR);
}

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

static void test_iso_datetime_7()
{
    uint8_t buf[7];
    time_t t, t2;
    struct tm tp;
    
    strptime("01-03-1976 13:27:45", "%d-%m-%Y %T", &tp);
    t = mktime(&tp);
    
    iso_datetime_7(buf, t);
    CU_ASSERT_EQUAL( buf[0], 76 ); /* year since 1900 */
    CU_ASSERT_EQUAL( buf[1], 3 ); /* month */
    CU_ASSERT_EQUAL( buf[2], 1 ); /* day */
    CU_ASSERT_EQUAL( buf[3], 13 ); /* hour */
    CU_ASSERT_EQUAL( buf[4], 27 ); /* minute */
    CU_ASSERT_EQUAL( buf[5], 45 ); /* second */
    /* the offset depends on current timezone and it's not easy to test */
    //CU_ASSERT_EQUAL( buf[6], 4 ); /* 15 min offset */
    
    /* check that reading returns the same time */
    t2 = iso_datetime_read_7(buf);
    CU_ASSERT_EQUAL(t2, t);
    
    //TODO check with differnt timezones for reading and writting
    
}

static void test_iso_1_dirid()
{
    char *dir;
    dir = iso_1_dirid("dir1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);
    dir = iso_1_dirid("dIR1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);
    dir = iso_1_dirid("DIR1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);
    dir = iso_1_dirid("dirwithbigname");
    CU_ASSERT_STRING_EQUAL(dir, "DIRWITHB");
    free(dir);
    dir = iso_1_dirid("dirwith8");
    CU_ASSERT_STRING_EQUAL(dir, "DIRWITH8");
    free(dir);
    dir = iso_1_dirid("dir.1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR_1");
    free(dir);
    dir = iso_1_dirid("4f<0KmM::xcvf");
    CU_ASSERT_STRING_EQUAL(dir, "4F_0KMM_");
    free(dir);
}

static void test_iso_2_dirid()
{
    char *dir;
    dir = iso_2_dirid("dir1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);
    dir = iso_2_dirid("dIR1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);
    dir = iso_2_dirid("DIR1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);
    dir = iso_2_dirid("dirwithbigname");
    CU_ASSERT_STRING_EQUAL(dir, "DIRWITHBIGNAME");
    free(dir);
    dir = iso_2_dirid("dirwith8");
    CU_ASSERT_STRING_EQUAL(dir, "DIRWITH8");
    free(dir);
    dir = iso_2_dirid("dir.1");
    CU_ASSERT_STRING_EQUAL(dir, "DIR_1");
    free(dir);
    dir = iso_2_dirid("4f<0KmM::xcvf");
    CU_ASSERT_STRING_EQUAL(dir, "4F_0KMM__XCVF");
    free(dir);
    dir = iso_2_dirid("directory with 31 characters ok");
    CU_ASSERT_STRING_EQUAL(dir, "DIRECTORY_WITH_31_CHARACTERS_OK");
    free(dir);
    dir = iso_2_dirid("directory with more than 31 characters");
    CU_ASSERT_STRING_EQUAL(dir, "DIRECTORY_WITH_MORE_THAN_31_CHA");
    free(dir);
}

static void test_iso_1_fileid()
{
    char *file;
    file = iso_1_fileid("file1");
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    file = iso_1_fileid("fILe1");
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    file = iso_1_fileid("FILE1");
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    file = iso_1_fileid(".EXT");
    CU_ASSERT_STRING_EQUAL(file, ".EXT");
    free(file);
    file = iso_1_fileid("file.ext");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_1_fileid("fiLE.ext");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_1_fileid("file.EXt");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_1_fileid("FILE.EXT");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_1_fileid("bigfilename");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILEN.");
    free(file);
    file = iso_1_fileid("bigfilename.ext");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILEN.EXT");
    free(file);
    file = iso_1_fileid("bigfilename.e");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILEN.E");
    free(file);
    file = iso_1_fileid("file.bigext");
    CU_ASSERT_STRING_EQUAL(file, "FILE.BIG");
    free(file);
    file = iso_1_fileid(".bigext");
    CU_ASSERT_STRING_EQUAL(file, ".BIG");
    free(file);
    file = iso_1_fileid("bigfilename.bigext");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILEN.BIG");
    free(file);
    file = iso_1_fileid("file<:a.ext");
    CU_ASSERT_STRING_EQUAL(file, "FILE__A.EXT");
    free(file);
    file = iso_1_fileid("file.<:a");
    CU_ASSERT_STRING_EQUAL(file, "FILE.__A");
    free(file);
    file = iso_1_fileid("file<:a.--a");
    CU_ASSERT_STRING_EQUAL(file, "FILE__A.__A");
    free(file);
    file = iso_1_fileid("file.ex1.ex2");
    CU_ASSERT_STRING_EQUAL(file, "FILE_EX1.EX2");
    free(file);
    file = iso_1_fileid("file.ex1.ex2.ex3");
    CU_ASSERT_STRING_EQUAL(file, "FILE_EX1.EX3");
    free(file);
    file = iso_1_fileid("fil.ex1.ex2.ex3");
    CU_ASSERT_STRING_EQUAL(file, "FIL_EX1_.EX3");
    free(file);
}

static void test_iso_2_fileid()
{
    char *file;
    file = iso_2_fileid("file1");
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    file = iso_2_fileid("fILe1");
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    file = iso_2_fileid("FILE1");
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    file = iso_2_fileid(".EXT");
    CU_ASSERT_STRING_EQUAL(file, ".EXT");
    free(file);
    file = iso_2_fileid("file.ext");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_2_fileid("fiLE.ext");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_2_fileid("file.EXt");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_2_fileid("FILE.EXT");
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_2_fileid("bigfilename");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILENAME.");
    free(file);
    file = iso_2_fileid("bigfilename.ext");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILENAME.EXT");
    free(file);
    file = iso_2_fileid("bigfilename.e");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILENAME.E");
    free(file);
    file = iso_2_fileid("31 characters filename.extensio");
    CU_ASSERT_STRING_EQUAL(file, "31_CHARACTERS_FILENAME.EXTENSIO");
    free(file);
    file = iso_2_fileid("32 characters filename.extension");
    CU_ASSERT_STRING_EQUAL(file, "32_CHARACTERS_FILENAME.EXTENSIO");
    free(file);
    file = iso_2_fileid("more than 30 characters filename.extension");
    CU_ASSERT_STRING_EQUAL(file, "MORE_THAN_30_CHARACTERS_FIL.EXT");
    free(file);
    file = iso_2_fileid("file.bigext");
    CU_ASSERT_STRING_EQUAL(file, "FILE.BIGEXT");
    free(file);
    file = iso_2_fileid(".bigext");
    CU_ASSERT_STRING_EQUAL(file, ".BIGEXT");
    free(file);
    file = iso_2_fileid("bigfilename.bigext");
    CU_ASSERT_STRING_EQUAL(file, "BIGFILENAME.BIGEXT");
    free(file);
    file = iso_2_fileid("file<:a.ext");
    CU_ASSERT_STRING_EQUAL(file, "FILE__A.EXT");
    free(file);
    file = iso_2_fileid("file.<:a");
    CU_ASSERT_STRING_EQUAL(file, "FILE.__A");
    free(file);
    file = iso_2_fileid("file<:a.--a");
    CU_ASSERT_STRING_EQUAL(file, "FILE__A.__A");
    free(file);
    file = iso_2_fileid("file.ex1.ex2");
    CU_ASSERT_STRING_EQUAL(file, "FILE_EX1.EX2");
    free(file);
    file = iso_2_fileid("file.ex1.ex2.ex3");
    CU_ASSERT_STRING_EQUAL(file, "FILE_EX1_EX2.EX3");
    free(file);
    file = iso_2_fileid("fil.ex1.ex2.ex3");
    CU_ASSERT_STRING_EQUAL(file, "FIL_EX1_EX2.EX3");
    free(file);
    file = iso_2_fileid(".file.bigext");
    CU_ASSERT_STRING_EQUAL(file, "_FILE.BIGEXT");
    free(file);
}

static void test_iso_r_dirid()
{
    char *dir;

    dir = iso_r_dirid("dir1", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);

    dir = iso_r_dirid("dIR1", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);

    /* allow lowercase */
    dir = iso_r_dirid("dIR1", 31, 1);
    CU_ASSERT_STRING_EQUAL(dir, "dIR1");
    free(dir);
    dir = iso_r_dirid("dIR1", 31, 2);
    CU_ASSERT_STRING_EQUAL(dir, "dIR1");
    free(dir);

    dir = iso_r_dirid("DIR1", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIR1");
    free(dir);
    dir = iso_r_dirid("dirwithbigname", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIRWITHBIGNAME");
    free(dir);
    dir = iso_r_dirid("dirwith8", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIRWITH8");
    free(dir);

    /* dot is not allowed */
    dir = iso_r_dirid("dir.1", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIR_1");
    free(dir);
    dir = iso_r_dirid("dir.1", 31, 1);
    CU_ASSERT_STRING_EQUAL(dir, "dir_1");
    free(dir);
    dir = iso_r_dirid("dir.1", 31, 2);
    CU_ASSERT_STRING_EQUAL(dir, "dir.1");
    free(dir);

    dir = iso_r_dirid("4f<0KmM::xcvf", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "4F_0KMM__XCVF");
    free(dir);
    dir = iso_r_dirid("4f<0KmM::xcvf", 31, 1);
    CU_ASSERT_STRING_EQUAL(dir, "4f_0KmM__xcvf");
    free(dir);
    dir = iso_r_dirid("4f<0KmM::xcvf", 31, 2);
    CU_ASSERT_STRING_EQUAL(dir, "4f<0KmM::xcvf");
    free(dir);

    dir = iso_r_dirid("directory with 31 characters ok", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIRECTORY_WITH_31_CHARACTERS_OK");
    free(dir);
    dir = iso_r_dirid("directory with more than 31 characters", 31, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIRECTORY_WITH_MORE_THAN_31_CHA");
    free(dir);
    dir = iso_r_dirid("directory with more than 31 characters", 35, 0);
    CU_ASSERT_STRING_EQUAL(dir, "DIRECTORY_WITH_MORE_THAN_31_CHARACT");
    free(dir);
}

static void test_iso_r_fileid()
{
    char *file;
    
    /* force dot */
    file = iso_r_fileid("file1", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    
    /* and not */
    file = iso_r_fileid("file1", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "FILE1");
    free(file);
    
    /* allow lowercase */
    file = iso_r_fileid("file1", 30, 1, 0);
    CU_ASSERT_STRING_EQUAL(file, "file1");
    free(file);
    file = iso_r_fileid("file1", 30, 2, 0);
    CU_ASSERT_STRING_EQUAL(file, "file1");
    free(file);
    
    /* force d-char and dot */
    file = iso_r_fileid("fILe1", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    /* force d-char but not dot */
    file = iso_r_fileid("fILe1", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "FILE1");
    free(file);
    /* allow lower case but force dot */
    file = iso_r_fileid("fILe1", 30, 1, 1);
    CU_ASSERT_STRING_EQUAL(file, "fILe1.");
    free(file);
    
    file = iso_r_fileid("FILE1", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "FILE1.");
    free(file);
    file = iso_r_fileid(".EXT", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, ".EXT");
    free(file);
    file = iso_r_fileid(".EXT", 30, 1, 0);
    CU_ASSERT_STRING_EQUAL(file, ".EXT");
    free(file);
    
    file = iso_r_fileid("file.ext", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    
    /* not force dot is the same in this case */
    file = iso_r_fileid("fiLE.ext", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_r_fileid("fiLE.ext", 30, 2, 0);
    CU_ASSERT_STRING_EQUAL(file, "fiLE.ext");
    free(file);

    file = iso_r_fileid("file.EXt", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    file = iso_r_fileid("FILE.EXT", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "FILE.EXT");
    free(file);
    
    file = iso_r_fileid("31 characters filename.extensio", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "31_CHARACTERS_FILENAME.EXTENSIO");
    free(file);
    file = iso_r_fileid("32 characters filename.extension", 30, 0, 1);
    CU_ASSERT_STRING_EQUAL(file, "32_CHARACTERS_FILENAME.EXTENSIO");
    free(file);
    
    /* allow lowercase */
    file = iso_r_fileid("31 characters filename.extensio", 30, 1, 1);
    CU_ASSERT_STRING_EQUAL(file, "31_characters_filename.extensio");
    free(file);
    
    /* and all characters */
    file = iso_r_fileid("31 characters filename.extensio", 30, 2, 1);
    CU_ASSERT_STRING_EQUAL(file, "31 characters filename.extensio");
    free(file);
    
    file = iso_r_fileid("more than 30 characters filename.extension", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "MORE_THAN_30_CHARACTERS_FIL.EXT");
    
    /* incrementing the size... */
    file = iso_r_fileid("more than 30 characters filename.extension", 35, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "MORE_THAN_30_CHARACTERS_FILENAME.EXT");
    file = iso_r_fileid("more than 30 characters filename.extension", 36, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "MORE_THAN_30_CHARACTERS_FILENAME.EXTE");
    
    free(file);
    file = iso_r_fileid("file.bigext", 30, 1, 0);
    CU_ASSERT_STRING_EQUAL(file, "file.bigext");
    free(file);
    file = iso_r_fileid(".bigext", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, ".BIGEXT");
    
    /* "strange" characters */
    file = iso_r_fileid("file<:a.ext", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "FILE__A.EXT");
    free(file);
    file = iso_r_fileid("file<:a.ext", 30, 1, 0);
    CU_ASSERT_STRING_EQUAL(file, "file__a.ext");
    free(file);
    file = iso_r_fileid("file<:a.ext", 30, 2, 0);
    CU_ASSERT_STRING_EQUAL(file, "file<:a.ext");
    free(file);
    
    /* multiple dots */
    file = iso_r_fileid("fi.le.a.ext", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "FI_LE_A.EXT");
    free(file);
    file = iso_r_fileid("fi.le.a.ext", 30, 1, 0);
    CU_ASSERT_STRING_EQUAL(file, "fi_le_a.ext");
    free(file);
    file = iso_r_fileid("fi.le.a.ext", 30, 2, 0);
    CU_ASSERT_STRING_EQUAL(file, "fi.le.a.ext");
    
    file = iso_r_fileid("file.<:a", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "FILE.__A");
    free(file);
    file = iso_r_fileid("file<:a.--a", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "FILE__A.__A");
    free(file);

    file = iso_r_fileid(".file.bigext", 30, 0, 0);
    CU_ASSERT_STRING_EQUAL(file, "_FILE.BIGEXT");
    free(file);

    file = iso_r_fileid(".file.bigext", 30, 2, 0);
    CU_ASSERT_STRING_EQUAL(file, ".file.bigext");
    free(file);
}

static void test_iso_rbtree_insert()
{
    int res;
    IsoRBTree *tree;
    char *str1, *str2, *str3, *str4, *str5;
    void *str;

    res = iso_rbtree_new((compare_function_t)strcmp, &tree);
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

void test_iso_htable_put_get()
{
    int res;
    IsoHTable *table;
    char *str1, *str2, *str3, *str4, *str5;
    void *str;
    
    res = iso_htable_create(4, iso_str_hash, (compare_function_t)strcmp, &table);
    CU_ASSERT_EQUAL(res, 1);

    /* try to get str from empty table */
    res = iso_htable_get(table, "first str", &str);
    CU_ASSERT_EQUAL(res, 0);
    
    /* ok, insert one str  */
    str1 = "first str";
    res = iso_htable_put(table, str1, str1);
    CU_ASSERT_EQUAL(res, 1);

    /* and now get str from table */
    res = iso_htable_get(table, "first str", &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str1);
    res = iso_htable_get(table, "second str", &str);
    CU_ASSERT_EQUAL(res, 0);

    str2 = "second str";
    res = iso_htable_put(table, str2, str2);
    CU_ASSERT_EQUAL(res, 1);
    
    str = NULL;
    res = iso_htable_get(table, "first str", &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str1);
    res = iso_htable_get(table, "second str", &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str2);

    /* insert again, with same key but other data */
    res = iso_htable_put(table, str2, str1);
    CU_ASSERT_EQUAL(res, 0);
    
    res = iso_htable_get(table, "second str", &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str2);

    str3 = "third str";
    res = iso_htable_put(table, str3, str3);
    CU_ASSERT_EQUAL(res, 1);

    str4 = "four str";
    res = iso_htable_put(table, str4, str4);
    CU_ASSERT_EQUAL(res, 1);

    str5 = "fifth str";
    res = iso_htable_put(table, str5, str5);
    CU_ASSERT_EQUAL(res, 1);

    /* some searches */
    res = iso_htable_get(table, "sixth str", &str);
    CU_ASSERT_EQUAL(res, 0);
    
    res = iso_htable_get(table, "fifth str", &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str5);
    
    iso_htable_destroy(table, NULL);
}

void add_util_suite()
{
    CU_pSuite pSuite = CU_add_suite("UtilSuite", NULL, NULL);

    CU_add_test(pSuite, "strconv()", test_strconv);
    CU_add_test(pSuite, "div_up()", test_div_up);
    CU_add_test(pSuite, "round_up()", test_round_up);
    CU_add_test(pSuite, "iso_bb()", test_iso_bb);
    CU_add_test(pSuite, "iso_lsb/msb()", test_iso_lsb_msb);
    CU_add_test(pSuite, "iso_read_lsb/msb()", test_iso_read_lsb_msb);
    CU_add_test(pSuite, "iso_datetime_7()", test_iso_datetime_7);
    CU_add_test(pSuite, "iso_1_dirid()", test_iso_1_dirid);
    CU_add_test(pSuite, "iso_2_dirid()", test_iso_2_dirid);
    CU_add_test(pSuite, "iso_1_fileid()", test_iso_1_fileid);
    CU_add_test(pSuite, "iso_2_fileid()", test_iso_2_fileid);
    CU_add_test(pSuite, "iso_r_dirid()", test_iso_r_dirid);
    CU_add_test(pSuite, "iso_r_fileid()", test_iso_r_fileid);
    CU_add_test(pSuite, "iso_rbtree_insert()", test_iso_rbtree_insert);
    CU_add_test(pSuite, "iso_htable_put/get()", test_iso_htable_put_get);
}
