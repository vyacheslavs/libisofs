/*
 * Unit test for image.h
 */


#include "libisofs.h"
#include "test.h"
#include "image.h"

#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

 
static void test_iso_image_new()
{
    int ret;
	IsoImage *image;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_PTR_NOT_NULL(image);
	CU_ASSERT_EQUAL(image->refcount, 1);
	CU_ASSERT_PTR_NOT_NULL(image->root);
	
	CU_ASSERT_STRING_EQUAL(image->volume_id, "volume_id");
	CU_ASSERT_STRING_EQUAL(image->volset_id, "volume_id");
	
	CU_ASSERT_PTR_NULL(image->publisher_id);
    CU_ASSERT_PTR_NULL(image->data_preparer_id);
    CU_ASSERT_PTR_NULL(image->system_id);
	CU_ASSERT_PTR_NULL(image->application_id);
	CU_ASSERT_PTR_NULL(image->copyright_file_id);
	CU_ASSERT_PTR_NULL(image->abstract_file_id);
	CU_ASSERT_PTR_NULL(image->biblio_file_id);
	
	//CU_ASSERT_PTR_NULL(image->bootcat);

	iso_image_unref(image);
}

static void test_iso_image_set_volume_id()
{
	int ret;
    IsoImage *image;
    char *volid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_STRING_EQUAL(image->volume_id, "volume_id");
	
	volid = "new volume id";
	iso_image_set_volume_id(image, volid);
	CU_ASSERT_STRING_EQUAL(image->volume_id, "new volume id");
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL(image->volume_id, volid);
	iso_image_unref(image);
}

static void test_iso_image_get_volume_id()
{
	int ret;
    IsoImage *image;
    char *volid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_STRING_EQUAL(iso_image_get_volume_id(image), "volume_id");
	
	volid = "new volume id";
	iso_image_set_volume_id(image, volid);
	CU_ASSERT_STRING_EQUAL( iso_image_get_volume_id(image), "new volume id" );
	
	iso_image_unref(image);
}

static void test_iso_image_set_publisher_id()
{
	int ret;
    IsoImage *image;
    char *pubid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->publisher_id);
	
	pubid = "new publisher id";
	iso_image_set_publisher_id(image, pubid);
	CU_ASSERT_STRING_EQUAL( image->publisher_id, "new publisher id" );
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL( image->publisher_id, pubid );
	iso_image_unref(image);
}

static void test_iso_image_get_publisher_id()
{
	int ret;
    IsoImage *image;
    char *pubid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->publisher_id);
	
	pubid = "new publisher id";
	iso_image_set_publisher_id(image, pubid);
	CU_ASSERT_STRING_EQUAL(iso_image_get_publisher_id(image), "new publisher id");
	
	iso_image_unref(image);
}

static void test_iso_image_set_data_preparer_id()
{
	int ret;
    IsoImage *image;
	char *dpid;
    
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->data_preparer_id);
	
	dpid = "new data preparer id";
	iso_image_set_data_preparer_id(image, dpid);
	CU_ASSERT_STRING_EQUAL(image->data_preparer_id, "new data preparer id");
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL(image->data_preparer_id, dpid);
	iso_image_unref(image);
}

static void test_iso_image_get_data_preparer_id()
{
	int ret;
    IsoImage *image;
    char *dpid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->data_preparer_id);
    
	dpid = "new data preparer id";
	iso_image_set_data_preparer_id(image, dpid);
	CU_ASSERT_STRING_EQUAL( iso_image_get_data_preparer_id(image), "new data preparer id" );
	
	iso_image_unref(image);
}

static void test_iso_image_set_system_id()
{
	int ret;
    IsoImage *image;
    char *sysid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->system_id);
	
	sysid = "new system id";
	iso_image_set_system_id(image, sysid);
	CU_ASSERT_STRING_EQUAL( image->system_id, "new system id" );
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL( image->system_id, sysid );
	iso_image_unref(image);
}

static void test_iso_image_get_system_id()
{
	int ret;
    IsoImage *image;
    char *sysid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(iso_image_get_system_id(image));
	
	sysid = "new system id";
	iso_image_set_system_id(image, sysid);
	CU_ASSERT_STRING_EQUAL( iso_image_get_system_id(image), "new system id" );
	
	iso_image_unref(image);
}

static void test_iso_image_set_application_id()
{
	int ret;
    IsoImage *image;
    char *appid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->application_id);
	
	appid = "new application id";
	iso_image_set_application_id(image, appid);
	CU_ASSERT_STRING_EQUAL( image->application_id, "new application id" );
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL( image->application_id, appid );
	iso_image_unref(image);
}

static void test_iso_image_get_application_id()
{
	int ret;
    IsoImage *image;
    char *appid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(iso_image_get_application_id(image));
	
	appid = "new application id";
	iso_image_set_application_id(image, appid);
	CU_ASSERT_STRING_EQUAL( iso_image_get_application_id(image), "new application id" );
	
	iso_image_unref(image);
}

static void test_iso_image_set_copyright_file_id()
{
	int ret;
    IsoImage *image;
    char *copid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->copyright_file_id);
	
	copid = "new copyright id";
	iso_image_set_copyright_file_id(image, copid);
	CU_ASSERT_STRING_EQUAL( image->copyright_file_id, "new copyright id" );
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL( image->copyright_file_id, copid );
	iso_image_unref(image);
}

static void test_iso_image_get_copyright_file_id()
{
	int ret;
    IsoImage *image;
    char *copid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(iso_image_get_copyright_file_id(image));
	
	copid = "new copyright id";
	iso_image_set_copyright_file_id(image, copid);
	CU_ASSERT_STRING_EQUAL( iso_image_get_copyright_file_id(image), "new copyright id" );
	
	iso_image_unref(image);
}

static void test_iso_image_set_abstract_file_id()
{
	int ret;
    IsoImage *image;
    char *absid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->abstract_file_id);
	
	absid = "new abstract id";
	iso_image_set_abstract_file_id(image, absid);
	CU_ASSERT_STRING_EQUAL( image->abstract_file_id, "new abstract id" );
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL( image->abstract_file_id, absid );
	iso_image_unref(image);
}

static void test_iso_image_get_abstract_file_id()
{
	int ret;
    IsoImage *image;
    char *absid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(iso_image_get_abstract_file_id(image));
	
	absid = "new abstract id";
	iso_image_set_abstract_file_id(image, absid);
	CU_ASSERT_STRING_EQUAL(iso_image_get_abstract_file_id(image), "new abstract id");
	
	iso_image_unref(image);
}

static void test_iso_image_set_biblio_file_id()
{
	int ret;
    IsoImage *image;
    char *bibid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(image->biblio_file_id);
	
	bibid = "new biblio id";
	iso_image_set_biblio_file_id(image, bibid);
	CU_ASSERT_STRING_EQUAL( image->biblio_file_id, "new biblio id" );
	
	/* check string was strdup'ed */
	CU_ASSERT_PTR_NOT_EQUAL( image->biblio_file_id, bibid );
	iso_image_unref(image);
}

static void test_iso_image_get_biblio_file_id()
{
	int ret;
    IsoImage *image;
    char *bibid;
	
	ret = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_PTR_NULL(iso_image_get_biblio_file_id(image));
	
	bibid = "new biblio id";
	iso_image_set_biblio_file_id(image, bibid);
	CU_ASSERT_STRING_EQUAL(iso_image_get_biblio_file_id(image), "new biblio id");
	
	iso_image_unref(image);
}

void add_image_suite()
{
	CU_pSuite pSuite = CU_add_suite("imageSuite", NULL, NULL);
	
	CU_add_test(pSuite, "iso_image_new()", test_iso_image_new);
	CU_add_test(pSuite, "iso_image_set_volume_id()", test_iso_image_set_volume_id);
	CU_add_test(pSuite, "iso_image_get_volume_id()", test_iso_image_get_volume_id);
	CU_add_test(pSuite, "iso_image_set_publisher_id()", test_iso_image_set_publisher_id);
	CU_add_test(pSuite, "iso_image_get_publisher_id()", test_iso_image_get_publisher_id);
	CU_add_test(pSuite, "iso_image_set_data_preparer_id()", test_iso_image_set_data_preparer_id);
	CU_add_test(pSuite, "iso_image_get_data_preparer_id()", test_iso_image_get_data_preparer_id);
	CU_add_test(pSuite, "iso_image_set_system_id()", test_iso_image_set_system_id);
	CU_add_test(pSuite, "iso_image_get_system_id()", test_iso_image_get_system_id);
	CU_add_test(pSuite, "iso_image_set_application_id()", test_iso_image_set_application_id);
	CU_add_test(pSuite, "iso_image_get_application_id()", test_iso_image_get_application_id);
	CU_add_test(pSuite, "iso_image_set_copyright_file_id()", test_iso_image_set_copyright_file_id);
	CU_add_test(pSuite, "iso_image_get_copyright_file_id()", test_iso_image_get_copyright_file_id);
	CU_add_test(pSuite, "iso_image_set_abstract_file_id()", test_iso_image_set_abstract_file_id);
	CU_add_test(pSuite, "iso_image_get_abstract_file_id()", test_iso_image_get_abstract_file_id);
	CU_add_test(pSuite, "iso_image_set_biblio_file_id()", test_iso_image_set_biblio_file_id);
	CU_add_test(pSuite, "iso_image_get_biblio_file_id()", test_iso_image_get_biblio_file_id);
}
