/* Copyright (c) 2015-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "test-common.h"
#include "hex-binary.h"
#include "settings.h"
#include "master-service.h"
#include "dict.h"
#include "test-mail-storage-common.h"
#include "dcrypt.h"

#include "mail-crypt-common.h"
#include "mail-crypt-key.h"
#include "mail-crypt-plugin.h"

static const char *mcp_old_user_key = "1\t716\t0\t048FD04FD3612B22D32790C592CF21CEF417EFD2EA34AE5F688FA5B51BED29E05A308B68DA78E16E90B47A11E133BD9A208A2894FD01B0BEE865CE339EA3FB17AC\td0cfaca5d335f9edc41c84bb47465184cb0e2ec3931bebfcea4dd433615e77a0";
static const char *mcp_old_user_key_id = "d0cfaca5d335f9edc41c84bb47465184cb0e2ec3931bebfcea4dd433615e77a0";
static const char *mcp_old_box_key = "1\t716\t1\t0567e6bf9579813ae967314423b0fceb14bda24749303923de9a9bb9370e0026f995901a57e63113eeb2baf0c940e978d00686cbb52bd5014bc318563375876255\t0300E46DA2125427BE968EB3B649910CDC4C405E5FFDE18D433A97CABFEE28CEEFAE9EE356C792004FFB80981D67E741B8CC036A34235A8D2E1F98D1658CFC963D07EB\td0cfaca5d335f9edc41c84bb47465184cb0e2ec3931bebfcea4dd433615e77a0\t7c9a1039ea2e4fed73e81dd3ffc3fa22ea4a28352939adde7bf8ea858b00fa4f";
static const char *mcp_old_box_key_id = "7c9a1039ea2e4fed73e81dd3ffc3fa22ea4a28352939adde7bf8ea858b00fa4f";

static struct test_mail_storage_ctx *test_ctx;
static const char *test_user_key_id;
static const char *test_box_key_id;

static struct mail_crypt_user mail_crypt_user;
static struct crypt_settings mail_crypt_settings = {
	.crypt_user_key_curve = "prime256v1",
	.crypt_user_key_password = "",
};

struct mail_crypt_user *mail_crypt_get_mail_crypt_user(struct mail_user *user ATTR_UNUSED)
{
	return &mail_crypt_user;
}

static
int test_mail_attribute_get(struct mailbox *box, bool user_key, bool shared,
			    const char *pubid, const char **value_r, const char **error_r)
{
	const char *attr_name;
	enum mail_attribute_type attr_type;

	if (strcmp(pubid, ACTIVE_KEY_NAME) == 0) {
		attr_name = user_key ? USER_CRYPT_PREFIX ACTIVE_KEY_NAME :
					BOX_CRYPT_PREFIX ACTIVE_KEY_NAME;
		attr_type = MAIL_ATTRIBUTE_TYPE_SHARED;
	} else {
		attr_name = t_strdup_printf("%s%s%s",
						user_key ? USER_CRYPT_PREFIX :
						  BOX_CRYPT_PREFIX,
						shared ? PUBKEYS_PREFIX :
						  PRIVKEYS_PREFIX,
						pubid);
		attr_type = shared ? MAIL_ATTRIBUTE_TYPE_SHARED : MAIL_ATTRIBUTE_TYPE_PRIVATE;
	}
	struct mail_attribute_value value;

	int ret;

	if ((ret = mailbox_attribute_get(box, attr_type,
					 attr_name, &value)) <= 0) {
		if (ret < 0) {
			*error_r = t_strdup_printf("mailbox_attribute_get(%s, %s) failed: %s",
						   mailbox_get_vname(box),
						   attr_name,
						   mailbox_get_last_internal_error(box, NULL));
		}
	} else {
		*value_r = t_strdup(value.value);
	}
	return ret;
}

static int
test_mail_attribute_set(struct mailbox_transaction_context *t,
			bool user_key, bool shared, const char *pubid,
			const char *value, const char **error_r)
{
	const char *attr_name;
	enum mail_attribute_type attr_type;

	if (strcmp(pubid, ACTIVE_KEY_NAME) == 0) {
		attr_name = user_key ? USER_CRYPT_PREFIX ACTIVE_KEY_NAME :
					BOX_CRYPT_PREFIX ACTIVE_KEY_NAME;
		attr_type = MAIL_ATTRIBUTE_TYPE_SHARED;
	} else {
		attr_name = t_strdup_printf("%s%s%s",
						user_key ? USER_CRYPT_PREFIX :
						  BOX_CRYPT_PREFIX,
						shared ? PUBKEYS_PREFIX :
						  PRIVKEYS_PREFIX,
						pubid);
		attr_type = shared ? MAIL_ATTRIBUTE_TYPE_SHARED : MAIL_ATTRIBUTE_TYPE_PRIVATE;
	}

	struct mail_attribute_value attr_value;

	int ret;

	i_zero(&attr_value);
	attr_value.value = value;

	if ((ret = mailbox_attribute_set(t, attr_type,
					 attr_name, &attr_value)) <= 0) {
		if (ret < 0) {
			*error_r = t_strdup_printf("mailbox_attribute_set(%s, %s) failed: %s",
						   mailbox_get_vname(mailbox_transaction_get_mailbox(t)),
						   attr_name,
						   mailbox_get_last_internal_error(mailbox_transaction_get_mailbox(t), NULL));
		}
	}

	return ret;
}


static void test_generate_user_key(void)
{
	struct dcrypt_keypair pair;
	const char *pubid;
	const char *error = NULL;

	test_begin("generate user key");

	/* try to generate a keypair for user */
	if (mail_crypt_user_generate_keypair(test_ctx->user, &pair,
					     &pubid, &error) < 0) {
		i_error("generate_keypair failed: %s", error);
		test_exit(1);
	}

	test_assert(pubid != NULL);

	test_user_key_id = p_strdup(test_ctx->pool, pubid);

	dcrypt_keypair_unref(&pair);
	error = NULL;

	/* keys ought to be in cache or somewhere...*/
	if (mail_crypt_user_get_private_key(test_ctx->user, NULL, &pair.priv, &error) <= 0)
	{
		i_error("Cannot get user private key: %s", error);
	}

	test_assert(pair.priv != NULL);

	if (pair.priv != NULL)
		dcrypt_key_unref_private(&pair.priv);

	test_end();
}

static void test_generate_inbox_key(void)
{
	struct dcrypt_public_key *user_key;
	struct dcrypt_keypair pair;
	const char *error = NULL, *pubid = NULL;

	test_begin("generate inbox key");

	if (mail_crypt_user_get_public_key(test_ctx->user, &user_key,
					    &error) <= 0) {
		i_error("Cannot get user private key: %s", error);
	}
	struct mail_namespace *ns =
		mail_namespace_find_inbox(test_ctx->user->namespaces);
	struct mailbox *box = mailbox_alloc(ns->list, "INBOX",
					    MAILBOX_FLAG_READONLY);
	if (mailbox_open(box) < 0)
		i_fatal("mailbox_open(INBOX) failed: %s",
			mailbox_get_last_internal_error(box, NULL));
	if (mail_crypt_box_generate_keypair(box, &pair, user_key, &pubid,
					    &error) < 0) {
		i_error("generate_keypair failed: %s", error);
		test_exit(1);
	}

	i_assert(pubid != NULL);

	dcrypt_keypair_unref(&pair);
	dcrypt_key_unref_public(&user_key);
	mailbox_free(&box);

	test_box_key_id = p_strdup(test_ctx->pool, pubid);

	test_end();
}

static void test_cache_reset(void)
{
	struct dcrypt_keypair pair;
	const char *error = NULL;

	test_begin("cache reset");

	struct mail_crypt_user *muser =
		mail_crypt_get_mail_crypt_user(test_ctx->user);
	mail_crypt_key_cache_destroy(&muser->key_cache);

	test_assert(mail_crypt_user_get_private_key(test_ctx->user, NULL,
						    &pair.priv, &error) > 0);
	if (error != NULL)
		i_error("mail_crypt_user_get_private_key() failed: %s", error);
	error = NULL;
	test_assert(mail_crypt_user_get_public_key(test_ctx->user,
						   &pair.pub, &error) > 0);
	if (error != NULL)
		i_error("mail_crypt_user_get_public_key() failed: %s", error);

	dcrypt_keypair_unref(&pair);

	test_end();
}

static void test_verify_keys(void)
{
	const char *value = "", *error = NULL;

	const char *enc_id;
	enum dcrypt_key_encryption_type enc_type;

	test_begin("verify keys");

	struct dcrypt_private_key *privkey = NULL, *user_key = NULL;
	struct dcrypt_public_key *pubkey = NULL;

	struct mail_namespace *ns =
		mail_namespace_find_inbox(test_ctx->user->namespaces);
	struct mailbox *box = mailbox_alloc(ns->list, "INBOX",
					    MAILBOX_FLAG_READONLY);
	if (mailbox_open(box) < 0)
		i_fatal("mailbox_open(INBOX) failed: %s",
			mailbox_get_last_internal_error(box, NULL));
	/* verify links */

	/* user's public key */
	test_assert(test_mail_attribute_get(box, TRUE, TRUE, ACTIVE_KEY_NAME,
		   &value, &error) > 0);
	test_assert(strcmp(value, test_user_key_id) == 0);

	test_assert(test_mail_attribute_get(box, TRUE, TRUE, value, &value,
		    &error) > 0);

	/* load key */
	test_assert(dcrypt_key_load_public(&pubkey, value, &error) == TRUE);

	/* see if it matches */
	test_assert(mail_crypt_public_key_id_match(pubkey, test_user_key_id,
						   &error) > 0);
	dcrypt_key_unref_public(&pubkey);

	/* user's private key */
	test_assert(test_mail_attribute_get(box, TRUE, FALSE, ACTIVE_KEY_NAME,
		   &value, &error) > 0);
	test_assert(strcmp(value, test_user_key_id) == 0);

	test_assert(test_mail_attribute_get(box, TRUE, FALSE, value, &value,
		    &error) > 0);

	/* load key */
	test_assert(dcrypt_key_load_private(&user_key, value, NULL, NULL,
					    &error) == TRUE);

	/* see if it matches */
	test_assert(mail_crypt_private_key_id_match(user_key, test_user_key_id,
						    &error) > 0);




	/* inbox's public key */
	test_assert(test_mail_attribute_get(box, FALSE, TRUE, ACTIVE_KEY_NAME,
		   &value, &error) > 0);
	test_assert(strcmp(value, test_box_key_id) == 0);

	test_assert(test_mail_attribute_get(box, FALSE, TRUE, value, &value,
		    &error) > 0);

	/* load key */
	test_assert(dcrypt_key_load_public(&pubkey, value, &error) == TRUE);

	/* see if it matches */
	test_assert(mail_crypt_public_key_id_match(pubkey, test_box_key_id,
						   &error) > 0);
	dcrypt_key_unref_public(&pubkey);

	/* user's private key */
		test_assert(test_mail_attribute_get(box, FALSE, FALSE, ACTIVE_KEY_NAME,
		   &value, &error) > 0);
	test_assert(strcmp(value, test_box_key_id) == 0);

	test_assert(test_mail_attribute_get(box, FALSE, FALSE, value, &value,
		    &error) > 0);

	test_assert(dcrypt_key_string_get_info(value, NULL, NULL, NULL,
						&enc_type, &enc_id, NULL,
						&error) == TRUE);

	test_assert(enc_type == DCRYPT_KEY_ENCRYPTION_TYPE_KEY);
	test_assert(strcmp(enc_id, test_user_key_id) == 0);

	/* load key */
	test_assert(dcrypt_key_load_private(&privkey, value, NULL, user_key,
					    &error) == TRUE);

	/* see if it matches */
	test_assert(mail_crypt_private_key_id_match(privkey, test_box_key_id,
						    &error) > 0);
	dcrypt_key_unref_private(&privkey);
	dcrypt_key_unref_private(&user_key);

	mailbox_free(&box);

	test_end();
}

static void test_old_key(void)
{
	test_begin("old keys");

	const char *error = NULL;
	struct dcrypt_private_key *privkey = NULL;

	struct mail_namespace *ns =
		mail_namespace_find_inbox(test_ctx->user->namespaces);
	struct mailbox *box = mailbox_alloc(ns->list, "INBOX",
					    MAILBOX_FLAG_READONLY);
	if (mailbox_open(box) < 0)
		i_fatal("mailbox_open(INBOX) failed: %s",
			mailbox_get_last_internal_error(box, NULL));

	struct mailbox_transaction_context *t =
		mailbox_transaction_begin(box, 0, __func__);

	test_mail_attribute_set(t, TRUE, FALSE, mcp_old_user_key_id,
				mcp_old_user_key, &error);
	test_mail_attribute_set(t, FALSE, FALSE, mcp_old_box_key_id,
				mcp_old_box_key, &error);

	(void)mailbox_transaction_commit(&t);

	error = NULL;

	/* try to load old key */
	test_assert(mail_crypt_get_private_key(box, mcp_old_box_key_id, FALSE, FALSE,
						&privkey, &error) > 0);

	if (error != NULL)
		i_error("mail_crypt_get_private_key(%s) failed: %s",
			mcp_old_box_key_id,
			error);

	test_assert(privkey != NULL);

	if (privkey != NULL) {
		buffer_t *key_id = t_buffer_create(32);
		test_assert(dcrypt_key_id_private_old(privkey, key_id, &error));
		test_assert(strcmp(binary_to_hex(key_id->data, key_id->used), mcp_old_box_key_id) == 0);
		dcrypt_key_unref_private(&privkey);
	}

	mailbox_free(&box);

	test_end();
}

static void test_setup(void)
{
	struct dcrypt_settings set = {
		.module_dir = top_builddir "/src/lib-dcrypt/.libs"
	};
	const char *error;
	if (!dcrypt_initialize(NULL, &set, &error)) {
		i_info("No functional dcrypt backend found - skipping tests: %s", error);
		test_exit(0);
	}
	test_ctx = test_mail_storage_init();
	const char *username = "mcp_test@example.com";
	const char *const extra_input[] = {
		"mail_attribute/dict=file",
		"mail_attribute/dict/file/driver=file",
		t_strdup_printf("dict_file_path=%s/%s/dovecot-attributes",
				test_ctx->home_root, username),
		NULL
	};
	struct test_mail_storage_settings storage_set = {
		.username = username,
		.driver = "maildir",
		.hierarchy_sep = "/",
		.extra_input = extra_input,
	};
	test_mail_storage_init_user(test_ctx, &storage_set);
	mail_crypt_user.set = &mail_crypt_settings;

	mail_crypt_key_register_mailbox_internal_attributes();
}

static void test_teardown(void)
{
	struct mail_crypt_user *muser =
		mail_crypt_get_mail_crypt_user(test_ctx->user);
	mail_crypt_key_cache_destroy(&muser->key_cache);

	test_mail_storage_deinit_user(test_ctx);
	test_mail_storage_deinit(&test_ctx);
	dcrypt_deinitialize();
}

int main(int argc, char **argv)
{
	void (*tests[])(void)  = {
		test_setup,
		test_generate_user_key,
		test_generate_inbox_key,
		test_cache_reset,
		test_verify_keys,
		test_old_key,
		test_teardown,
		NULL
	};


	master_service = master_service_init("test-mail-key",
					     MASTER_SERVICE_FLAG_STANDALONE |
					     MASTER_SERVICE_FLAG_DONT_SEND_STATS |
					     MASTER_SERVICE_FLAG_CONFIG_BUILTIN |
					     MASTER_SERVICE_FLAG_NO_SSL_INIT |
					     MASTER_SERVICE_FLAG_NO_INIT_DATASTACK_FRAME,
					     &argc, &argv, "");
	settings_info_register(&dict_setting_parser_info);
	settings_info_register(&dict_file_setting_parser_info);
	int ret = test_run(tests);
	master_service_deinit(&master_service);
	return ret;
}
