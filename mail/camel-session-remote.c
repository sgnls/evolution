/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <mail-dbus.h>
#include <dbind.h>
#include <evo-dbus.h>
#include "camel-session-remote.h"
#include "camel-object-remote.h"

#define CAMEL_DBUS_NAME "org.gnome.evolution.camel"
extern GHashTable *store_hash;
GHashTable *store_rhash = NULL;
#define d(x) x

const char *session_str = "session";

/*
typedef enum {
	CAMEL_PROVIDER_STORE,
	CAMEL_PROVIDER_TRANSPORT,
	CAMEL_NUM_PROVIDER_TYPES
} CamelProviderType;

typedef enum {
	CAMEL_SESSION_ALERT_INFO,
	CAMEL_SESSION_ALERT_WARNING,
	CAMEL_SESSION_ALERT_ERROR
} CamelSessionAlertType;
*/

void
camel_session_remote_construct	(CamelSessionRemote *session,
			const char *storage_path)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_construct",
			&error, 
			"ss", session->object_id, storage_path); /* Just string of base dir */

	if (!ret) {
		g_warning ("Error: Constructing camel session: %s\n", error.message);
		return;
	}

	store_rhash = g_hash_table_new (g_direct_hash, g_direct_equal);
	d(printf("Camel session constructed remotely\n"));

}

char *
camel_session_remote_get_password (CamelSessionRemote *session,
			CamelObjectRemote *service,
			const char *domain,
			const char *prompt,
			const char *item,
			guint32 flags,
			CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *passwd, *err;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_password",
			&error, 
			"sssssu=>ss", session->object_id, service->object_id, domain, prompt, item, flags, &passwd, &err); 

	if (!ret) {
		g_warning ("Error: Camel session fetching password: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel session get password remotely\n"));

	return passwd;
}

char *
camel_session_remote_get_storage_path (CamelSessionRemote *session, CamelObjectRemote *service, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *storage_path, *err;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_storage_path",
			&error, 
			"ss=>ss", session->object_id, service->object_id, &storage_path, &err);

	if (!ret) {
		g_warning ("Error: Camel session fetching storage path: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel session get storage path remotely\n"));

	return storage_path;
}

void
camel_session_remote_forget_password (CamelSessionRemote *session, 
				CamelObjectRemote *service,
				const char *domain,
				const char *item,
				CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_forget_password",
			&error, 
			"ssss=>s", session->object_id, service->object_id, domain, item, &err); 

	if (!ret) {
		g_warning ("Error: Camel session forget password: %s\n", error.message);
		return;
	}

	d(printf("Camel session forget password remotely\n"));

	return;
}

CamelObjectRemote *
camel_session_remote_get_service (CamelSessionRemote *session, const char *url_string,
			   CamelProviderType type, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *service, *err;
	CamelObjectRemote *rstore;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_service",
			&error, 
			"si=>ss", url_string, type, &service, &err);

	if (!ret) {
		g_warning ("Error: Camel session get service: %s\n", error.message);
		return NULL;
	}

	rstore = g_new0 (CamelObjectRemote, 1);
	rstore->object_id = service;
	rstore->type = CAMEL_RO_STORE;
	rstore->hooks = NULL;
	d(printf("Camel session get service remotely\n"));
	g_hash_table_insert (store_rhash, g_hash_table_lookup(store_hash, service), rstore);
	return rstore;
}

CamelObjectRemote *
camel_session_remote_get_service_connected (CamelSessionRemote *session, const char *url_string,
			   CamelProviderType type, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *service, *err;
	CamelObjectRemote *rstore;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_service_connected",
			&error, 
			"si=>ss", url_string, type, &service, &err);

	if (!ret) {
		g_warning ("Error: Camel session get service connected: %s\n", error.message);
		return NULL;
	}

	rstore = g_new0 (CamelObjectRemote, 1);
	rstore->object_id = service;
	rstore->type = CAMEL_RO_STORE;
	rstore->hooks = NULL;
	d(printf("Camel session get service connected remotely\n"));
	g_hash_table_insert (store_rhash, g_hash_table_lookup(store_hash, service), rstore);
	return rstore;
}

gboolean  
camel_session_remote_alert_user (CamelSessionRemote *session, 
					CamelSessionAlertType type,
					const char *prompt,
					gboolean cancel)
{
	gboolean ret, success;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_alert_user",
			&error, 
			"sisi=>i", session->object_id, type, prompt, cancel, &success);

	if (!ret) {
		g_warning ("Error: Camel session alerting user: %s\n", error.message);
		return 0;
	}

	d(printf("Camel session alert user remotely\n"));

	return success;
}

char *
camel_session_remote_build_password_prompt (const char *type,
				     const char *user,
				     const char *host)
{
	gboolean ret;
	DBusError error;
	char *prompt;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_build_password_prompt",
			&error, 
			"sss=>s", type, user, host, &prompt);

	if (!ret) {
		g_warning ("Error: Camel session building password prompt: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel session build password prompt remotely\n"));

	return prompt;
}

gboolean           
camel_session_remote_is_online (CamelSessionRemote *session)
{
	gboolean ret, is_online;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_is_online",
			&error, 
			"s=>i", session->object_id, &is_online);

	if (!ret) {
		g_warning ("Error: Camel session check for online: %s\n", error.message);
		return 0;
	}

	d(printf("Camel session check for online remotely\n"));

	return is_online;

}

void 
camel_session_remote_set_online  (CamelSessionRemote *session,
				gboolean online)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_set_online",
			&error, 
			"si", "session", online);

	if (!ret) {
		g_warning ("Error: Camel session set online: %s\n", error.message);
		return;
	}

	d(printf("Camel session set online remotely\n"));

	return;
}

gboolean
camel_session_remote_check_junk (CamelSessionRemote *session)
{
	gboolean ret, check_junk;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_check_junk",
			&error, 
			"s=>i", session->object_id, &check_junk);

	if (!ret) {
		g_warning ("Error: Camel session check for junk: %s\n", error.message);
		return 0;
	}

	d(printf("Camel session check for junk remotely\n"));

	return check_junk;
}

void 
camel_session_remote_set_check_junk (CamelSessionRemote *session,
				   gboolean check_junk)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_set_check_junk",
			&error, 
			"si", session->object_id, check_junk);

	if (!ret) {
		g_warning ("Error: Camel session set check junk: %s\n", error.message);
		return;
	}

	d(printf("Camel session set check junk remotely\n"));

	return;
}

gboolean 
camel_session_remote_get_network_state (CamelSessionRemote *session)
{
	gboolean ret, network_state;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_network_state",
			&error, 
			"s=>i", session->object_id, &network_state);

	if (!ret) {
		g_warning ("Error: Camel session check for network state: %s\n", error.message);
		return 0;
	}

	d(printf("Camel session check for network state remotely\n"));

	return network_state;
}

void 
camel_session_remote_set_network_state  (CamelSessionRemote *session,
				     	gboolean network_state)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_set_network_state",
			&error, 
			"si", session_str, network_state);

	if (!ret) {
		g_warning ("Error: Camel session set network state: %s\n", error.message);
		return;
	}

	d(printf("Camel session set check network state remotely\n"));

	return;
}

