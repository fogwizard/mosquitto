/*
Copyright (c) 2009-2013 Roger Light <roger@atchoo.org>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of mosquitto nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef CMAKE
#endif

#include <mosquitto_broker.h>
#include <memory_mosq.h>

extern struct mosquitto_db int_db;


/* Options for logging should be:
 *
 * A combination of:
 * To a file
 * To stdout/stderr
 * To topics
 */

/* Give option of logging timestamp.
 * Logging pid.
 */
static int log_destinations = MQTT3_LOG_STDERR;
static int log_priorities = MOSQ_LOG_ERR | MOSQ_LOG_WARNING | MOSQ_LOG_NOTICE | MOSQ_LOG_INFO;

int mqtt3_log_init(int priorities, int destinations)
{
	int rc = 0;

	log_priorities = priorities;
	log_destinations = destinations;
	return rc;
}

int mqtt3_log_close(void)
{
	/* FIXME - do something for all destinations! */

	return MOSQ_ERR_SUCCESS;
}

int _mosquitto_log_printf(struct mosquitto *mosq, int priority, const char *fmt, ...)
{
	va_list va;
	char *s;
	char *st;
	int len;
	const char *topic;
	time_t now = time(NULL);

	if((log_priorities & priority) && log_destinations != MQTT3_LOG_NONE){
		switch(priority){
			case MOSQ_LOG_SUBSCRIBE:
				topic = "$SYS/broker/log/M/subscribe";
				break;
			case MOSQ_LOG_UNSUBSCRIBE:
				topic = "$SYS/broker/log/M/unsubscribe";
				break;
			case MOSQ_LOG_DEBUG:
				topic = "$SYS/broker/log/D";
				break;
			case MOSQ_LOG_ERR:
				topic = "$SYS/broker/log/E";
				break;
			case MOSQ_LOG_WARNING:
				topic = "$SYS/broker/log/W";
				break;
			case MOSQ_LOG_NOTICE:
				topic = "$SYS/broker/log/N";
				break;
			case MOSQ_LOG_INFO:
				topic = "$SYS/broker/log/I";
				break;
			default:
				topic = "$SYS/broker/log/E";
		}
		len = strlen(fmt) + 500;//够山寨,日志后面的可变参数部分最长限制为500个字符
		s = _mosquitto_malloc(len*sizeof(char));
		if(!s) return MOSQ_ERR_NOMEM;

		va_start(va, fmt);
		vsnprintf(s, len, fmt, va);
		va_end(va);
		s[len-1] = '\0'; /* Ensure string is null terminated. */

		if(log_destinations & MQTT3_LOG_STDOUT){
			if(int_db.config && int_db.config->log_timestamp){
				fprintf(stdout, "%d: %s\n", (int)now, s);
			}else{
				fprintf(stdout, "%s\n", s);
			}
			fflush(stdout);
		}
		if(log_destinations & MQTT3_LOG_STDERR){
			if(int_db.config && int_db.config->log_timestamp){
				fprintf(stderr, "%d: %s\n", (int)now, s);
			}else{
				fprintf(stderr, "%s\n", s);
			}
			fflush(stderr);
		}
		if(log_destinations & MQTT3_LOG_FILE && int_db.config->log_fptr){
			//直接fprintf到文件里面去，多线程岂不悲剧了,fprintf里面是有锁的，性能很低，而且程序一会挂了可能日志没有记录进去，这是最悲剧的
			if(int_db.config && int_db.config->log_timestamp){
				fprintf(int_db.config->log_fptr, "%d: %s\n", (int)now, s);
			}else{
				fprintf(int_db.config->log_fptr, "%s\n", s);
			}
		}
		if(log_destinations & MQTT3_LOG_TOPIC && priority != MOSQ_LOG_DEBUG){
			//这里是一个好功能，将指定消息发布，这样就可以方便的监控内部状态了
			if(int_db.config && int_db.config->log_timestamp){
				len += 30;
				st = _mosquitto_malloc(len*sizeof(char));
				if(!st){
					_mosquitto_free(s);
					return MOSQ_ERR_NOMEM;
				}
				snprintf(st, len, "%d: %s", (int)now, s);
				mqtt3_db_messages_easy_queue(&int_db, NULL, topic, 2, strlen(st), st, 0);
				_mosquitto_free(st);
			}else{
				mqtt3_db_messages_easy_queue(&int_db, NULL, topic, 2, strlen(s), s, 0);
			}
		}
		_mosquitto_free(s);
	}

	return MOSQ_ERR_SUCCESS;
}

