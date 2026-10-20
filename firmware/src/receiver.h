#ifndef LISTENM_RECEIVER_H
#define LISTENM_RECEIVER_H
// One connection; returns on disconnect/error. Caller retries with retained cursor.
int receiver_run(const char *host, unsigned port, const char *token, char last_id[96]);
#endif
