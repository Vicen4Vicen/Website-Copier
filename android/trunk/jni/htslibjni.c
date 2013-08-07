/*
HTTrack Android JAVA Native Interface Stubs.

HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <string.h>
#include <jni.h>
#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "htsglobal.h"
#include "htsbase.h"
#include "htsopt.h"
#include "httrack-library.h"
#include "htsdefines.h"
#include "htscore.h"

/* fine-grained stats */
#define GENERATE_FINE_STATS 1

/* redirect stdio on a log file ? */
#define REDIRECT_STDIO_LOG_FILE 1

/* Our own assert version. */
static void assert_failure(const char* exp, const char* file, int line) {
  /* FIXME TODO: pass the getExternalStorageDirectory() in init. */
  FILE *const dumpFile = fopen("/sdcard/HTTrack/error.txt", "wb");
  if (dumpFile != NULL) {
    fprintf(dumpFile, "assertion '%s' failed at %s:%d\n", exp, file, line);
    fclose(dumpFile);
  }
  abort();
}
#undef assert
#define assert(EXP) (void)( (EXP) || (assert_failure(#EXP, __FILE__, __LINE__), 0) )

static httrackp * global_opt = NULL;
static int global_opt_stop = 0;
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

#define MUTEX_LOCK(MUTEX) do {                  \
    if (pthread_mutex_lock(&MUTEX) != 0) {      \
      assert(! "pthread_mutex_lock failed");    \
    }                                           \
  } while(0)

#define MUTEX_UNLOCK(MUTEX) do {                \
    if (pthread_mutex_unlock(&MUTEX) != 0) {    \
      assert(! "pthread_mutex_unlock failed");  \
    }                                           \
  } while(0)

#define UNUSED(VAR) (void) VAR

/* HTTrackLib global class pointer. */
static jclass cls_HTTrackLib = NULL;
static jclass cls_HTTrackCallbacks = NULL;
static jclass cls_HTTrackStats = NULL;
static jclass cls_HTTrackStats_Element = NULL;

/* HTTrackStats default constructor. */
static jmethodID cons_HTTrackStats = NULL;
static jmethodID cons_HTTrackStats_Element = NULL;

/* HTTrackStats methods */
static jmethodID meth_HTTrackCallbacks_onRefresh = NULL;

/* The stats field */
static jfieldID field_callbacks = NULL;

/* The elements field */
static jfieldID field_elements = NULL;

/* List of fields (main stats). */
#define LIST_OF_FIELDS() \
DECLARE_FIELD(state); \
DECLARE_FIELD(completion); \
DECLARE_FIELD(linksScanned); \
DECLARE_FIELD(linksTotal); \
DECLARE_FIELD(linksBackground); \
DECLARE_FIELD(bytesReceived); \
DECLARE_FIELD(bytesWritten); \
DECLARE_FIELD(startTime); \
DECLARE_FIELD(elapsedTime); \
DECLARE_FIELD(bytesReceivedCompressed); \
DECLARE_FIELD(bytesReceivedUncompressed); \
DECLARE_FIELD(filesReceivedCompressed); \
DECLARE_FIELD(filesWritten); \
DECLARE_FIELD(filesUpdated); \
DECLARE_FIELD(filesWrittenBackground); \
DECLARE_FIELD(requestsCount); \
DECLARE_FIELD(socketsAllocated); \
DECLARE_FIELD(socketsCount); \
DECLARE_FIELD(errorsCount); \
DECLARE_FIELD(warningsCount); \
DECLARE_FIELD(infosCount); \
DECLARE_FIELD(totalTransferRate); \
DECLARE_FIELD(transferRate)

#define STRING "Ljava/lang/String;"

/* List of fields (Element). **/
#define LIST_OF_FIELDS_ELT() \
DECLARE_FIELD(STRING, address); \
DECLARE_FIELD(STRING, path); \
DECLARE_FIELD(STRING, filename); \
DECLARE_FIELD("Z", isUpdate); \
DECLARE_FIELD("I", state); \
DECLARE_FIELD("I", code); \
DECLARE_FIELD(STRING, message); \
DECLARE_FIELD("Z", isNotModified); \
DECLARE_FIELD("Z", isCompressed); \
DECLARE_FIELD("J", size); \
DECLARE_FIELD("J", totalSize); \
DECLARE_FIELD(STRING, mime); \
DECLARE_FIELD(STRING, charset)

/* Declare fields ids. */
#define DECLARE_FIELD(NAME) static jfieldID field_ ##NAME = NULL
LIST_OF_FIELDS();
#undef DECLARE_FIELD

/* Declare fields ids (stats). */
#define DECLARE_FIELD(TYPE, NAME) static jfieldID field_elt_ ##NAME = NULL
LIST_OF_FIELDS_ELT();
#undef DECLARE_FIELD

static jclass findClass(JNIEnv *env, const char *name) {
  jclass localClass = (*env)->FindClass(env, name);
  /* "Note however that the jclass is a class reference and must be protected
   * with a call to NewGlobalRef " -- DARN! */
  if (localClass != NULL) {
    return (*env)->NewGlobalRef(env, localClass);
  }
  return NULL;
}

static void releaseClass(JNIEnv *env, jclass *cls) {
  if (cls != NULL) {
    (*env)->DeleteGlobalRef(env, *cls);
    *cls = NULL;
  }
}

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  union {
    void *ptr;
    JNIEnv *env;
  } u;
  if ((*vm)->GetEnv(vm, &u.ptr, JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }
  UNUSED(reserved);

  /* HTTrackLib class */
  cls_HTTrackLib = findClass(u.env, "com/httrack/android/jni/HTTrackLib");
  assert(cls_HTTrackLib != NULL);
  cls_HTTrackCallbacks = findClass(u.env, "com/httrack/android/jni/HTTrackCallbacks");
  assert(cls_HTTrackCallbacks != NULL);
  cls_HTTrackStats = findClass(u.env, "com/httrack/android/jni/HTTrackStats");
  assert(cls_HTTrackStats != NULL);
  cls_HTTrackStats_Element = findClass(u.env, "com/httrack/android/jni/HTTrackStats$Element");
  assert(cls_HTTrackStats_Element != NULL);

  /* "Note that jfieldIDs and jmethodIDs are opaque types, not object
   * references, and should not be passed to NewGlobalRef" */

  /* Constructors */
  cons_HTTrackStats = (*u.env)->GetMethodID(u.env, cls_HTTrackStats, "<init>", "()V");
  assert(cons_HTTrackStats != NULL);
  cons_HTTrackStats_Element =
    (*u.env)->GetMethodID(u.env, cls_HTTrackStats_Element, "<init>", "()V");
  assert(cons_HTTrackStats_Element != NULL);

  /* Methods */
  meth_HTTrackCallbacks_onRefresh =
    (*u.env)->GetMethodID(u.env, cls_HTTrackCallbacks, "onRefresh",
                          "(Lcom/httrack/android/jni/HTTrackStats;)V");
  assert(meth_HTTrackCallbacks_onRefresh != NULL);

  /* The "callbacks" field */
  field_callbacks = (*u.env)->GetFieldID(u.env, cls_HTTrackLib, "callbacks",
                                         "Lcom/httrack/android/jni/HTTrackCallbacks;");
  assert(field_callbacks != NULL);

  /* Load HTTrackStats fields ids. */
#define DECLARE_FIELD(NAME) do {                                 \
  field_ ##NAME = (*u.env)->GetFieldID(u.env, cls_HTTrackStats,  \
                                       #NAME, "J");              \
  assert(field_ ##NAME != NULL);                                 \
} while(0)
  LIST_OF_FIELDS();
#undef DECLARE_FIELD

  /* The elements array */
  field_elements = (*u.env)->GetFieldID(u.env, cls_HTTrackStats, "elements",
                                        "[Lcom/httrack/android/jni/HTTrackStats$Element;");
  assert(field_elements != NULL);

  /* Load HTTrackStats fields (element) ids. */
#define DECLARE_FIELD(TYPE, NAME) do {                         \
  field_elt_ ##NAME = (*u.env)->GetFieldID(u.env,              \
                                           cls_HTTrackStats_Element,   \
                                           #NAME, TYPE);       \
  assert(field_elt_ ##NAME != NULL);                           \
} while(0)
  LIST_OF_FIELDS_ELT();
#undef DECLARE_FIELD

  /* Java VM 1.6 */
  return JNI_VERSION_1_6;
}

/* note: never called on Android */
void JNI_OnUnload(JavaVM *vm, void *reserved) {
  union {
    void *ptr;
    JNIEnv *env;
  } u;
  UNUSED(reserved);

  if ((*vm)->GetEnv(vm, &u.ptr, JNI_VERSION_1_6) != JNI_OK) {
    return ;
  }
  releaseClass(u.env, &cls_HTTrackLib);
  releaseClass(u.env, &cls_HTTrackCallbacks);
  releaseClass(u.env, &cls_HTTrackStats);
  releaseClass(u.env, &cls_HTTrackStats_Element);
}

/* FIXME -- This is dirty... we are supposed to keep the error message. */
static char *getSafeCopy(const char *message) {
  static char *buffer = NULL;
  static size_t capa = 0;
  const size_t size = strlen(message) + 1;
  if (capa < size) {
    for(capa = 16 ; capa < size ; capa <<= 1) ;
    buffer = realloc(buffer, capa);
    assert(buffer != NULL);
  }
  strcpy(buffer, message);
  return buffer;
}

static void throwException(JNIEnv* env, const char *exception,
    const char *message) {
  jclass cls = (*env)->FindClass(env, exception);
  assert(cls != NULL);
  (*env)->ThrowNew(env, cls, getSafeCopy(message));
}

static void throwRuntimeException(JNIEnv* env, const char *message) {
  throwException(env, "java/lang/RuntimeException", message);
}

static void throwIOException(JNIEnv* env, const char *message) {
  throwException(env, "java/io/IOException", message);
}

static void throwNPException(JNIEnv* env, const char *message) {
  throwException(env, "java/lang/NullPointerException", message);
}

void Java_com_httrack_android_jni_HTTrackLib_init(JNIEnv* env, jclass clazz) {
  /* redirect stdout and stderr to a log file for debugging purpose */
#if REDIRECT_STDIO_LOG_FILE
  FILE *const log = fopen("/sdcard/HTTrack/log.txt", "wb");
  if (log != NULL) {
    const int fd = fileno(log);
    if (dup2(fd, 1) == -1 || dup2(fd, 2) == -1) {
      assert(! "could not redirect stdin/stdout");
    }
  }
#endif

  /* created files shall be seen by users */
  umask(S_IWOTH);

  /* initialize library */
  assert(cls_HTTrackLib != NULL);
  hts_init();

  UNUSED(env);
  UNUSED(clazz);
}

jstring Java_com_httrack_android_jni_HTTrackLib_getVersion(JNIEnv* env, jclass clazz) {
  const char *version = hts_version();
  assert(version != NULL);
  UNUSED(clazz);
  return (*env)->NewStringUTF(env, version);
}

jstring Java_com_httrack_android_jni_HTTrackLib_getFeatures(JNIEnv* env, jclass clazz) {
  const char *features = hts_is_available();
  assert(features != NULL);
  UNUSED(clazz);
  return (*env)->NewStringUTF(env, features);
}

typedef struct jni_context_t {
  JNIEnv *env;
  /* HTTrackCallbacks object */
  jobject callbacks; 
} jni_context_t;

typedef enum hts_state_id_t {
  STATE_NONE = 0,
  STATE_RECEIVE,
  STATE_CONNECTING,
  STATE_DNS,
  STATE_FTP,
  STATE_READY,
  STATE_MAX
} hts_state_id_t;

typedef struct hts_state_t {
  size_t index;
  hts_state_id_t state;
  int code;
  const char *message;
} hts_state_t;

/* NewStringUTF, but ignore invalid UTF-8 or NULL input. */
static jobject newStringSafe(JNIEnv *env, const char *s) {
  if (s != NULL) {
    const int ne = ! (*env)->ExceptionOccurred(env);
    jobject str = (*env)->NewStringUTF(env, s);
    /* Silently ignore UTF-8 exception. */
    if (str == NULL && (*env)->ExceptionOccurred(env) && ne) {
      (*env)->ExceptionClear(env);
    }
    return str;
  }
  return NULL;
}

static jobject build_stats(jni_context_t *const t, httrackp * opt,
  lien_back * back, int back_max, int back_index, int lien_n,
  int lien_tot, int stat_time, hts_stat_struct * stats) {
#define STATE_MAX 32
  hts_state_t state[STATE_MAX];
  size_t index = 0;

  /* create stats object */
  jobject ostats = (*t->env)->NewObject(t->env, cls_HTTrackStats, cons_HTTrackStats);
  if (ostats == NULL) {
    return NULL;
  }

  assert(stats != NULL);

#define COPY_(VALUE, FIELD) do { \
  (*t->env)->SetLongField(t->env, ostats, field_ ##FIELD, \
      (jlong) (VALUE)); \
} while(0)
#define COPY(MEMBER, FIELD) COPY_(stats->MEMBER, FIELD)

  /* Copy stats */
  COPY(HTS_TOTAL_RECV, bytesReceived);
  COPY(rate, transferRate);
  COPY(stat_bytes, bytesWritten);
  COPY(stat_timestart, startTime);
  COPY(total_packed, bytesReceivedCompressed);
  COPY(total_unpacked, bytesReceivedUncompressed);
  COPY(total_packedfiles, filesReceivedCompressed);
  COPY(stat_files, filesWritten);
  COPY(stat_updated_files, filesUpdated);
  COPY(stat_background, filesWrittenBackground);
  COPY(stat_nrequests, requestsCount);
  COPY(stat_sockid, socketsAllocated);
  COPY(stat_nsocket, socketsCount);
  COPY(stat_errors, errorsCount);
  COPY(stat_warnings, warningsCount);
  COPY(stat_infos, infosCount);
  if (stat_time > 0 && stats->HTS_TOTAL_RECV > 0) {
    const jlong rate = (jlong) (stats->HTS_TOTAL_RECV / stat_time);
    COPY_(rate, totalTransferRate);
  }
  COPY(nbk, linksBackground);

  COPY_(hts_is_testing(opt), state);
  COPY_(hts_is_parsing(opt, -1), completion);
  COPY_(lien_n, linksScanned);
  COPY_(lien_tot, linksTotal);
  COPY_(stat_time, elapsedTime);

  /* Collect individual stats */
  if (back_index >= 0) {
    const size_t index_max = STATE_MAX;
    size_t k;
    /* current links first */
    for(k = 0, index = 0 ; k < 2 ; k++) {
      /* priority (receive first, then requests, then ready slots) */
      size_t j;
      for(j = 0; j < 3 && index < index_max ; j++) {
        size_t _i;
        /* when k=0, just take first item (possibly being parsed) */
        for(_i = k; _i < ( k == 0 ? 1 : (size_t) back_max ) && index < index_max; _i++) {        // no lien
          const size_t i = (back_index + _i) % back_max;
          /* active link */
          if (back[i].status >= 0) {
            /* Cleanup */
            state[index].state = STATE_NONE;
            state[index].code = 0;
            state[index].message = NULL;

            /* Check state */
            switch (j) {
            case 0:          // prioritaire
              if (back[i].status > 0 && back[i].status < 99) {
                state[index].state = STATE_RECEIVE;
              }
              break;
            case 1:
              if (back[i].status == STATUS_WAIT_HEADERS) {
                state[index].state = STATE_RECEIVE;
              } else if (back[i].status == STATUS_CONNECTING) {
                state[index].state = STATE_CONNECTING;
              } else if (back[i].status == STATUS_WAIT_DNS) {
                state[index].state = STATE_DNS;
              } else if (back[i].status == STATUS_FTP_TRANSFER) {
                state[index].state = STATE_FTP;
              }
              break;
            default:
              if (back[i].status == STATUS_READY) {   // prêt
                state[index].state = STATE_READY;
                state[index].code = back[i].r.statuscode;
                state[index].message = back[i].r.msg;
              }
              break;
            }

            /* Increment */
            if (state[index].state != STATE_NONE) {
              /* Take note of the offset */
              state[index].index = i;

              /* Next one */
              index++;
            }
          }
        }
      }
    }
  }

  if (GENERATE_FINE_STATS && back_index >= 0) {
    size_t i;

    /* Elements */
    jobjectArray elements =
      (*t->env)->NewObjectArray(t->env, index, cls_HTTrackStats_Element, NULL);
    if (elements == NULL) {
      return NULL;
    }

    /* Put elements in elements field of stats */
    (*t->env)->SetObjectField(t->env, ostats, field_elements, elements);

    /* Fill */
    for(i = 0 ; i < index ; i++) {
      /* Index to back[] */
      const int index = state[i].index;

      /* Create item */
      jobject element =
        (*t->env)->NewObject(t->env, cls_HTTrackStats_Element,
                             cons_HTTrackStats_Element);
      if (element == NULL) {
        return NULL;
      }
      (*t->env)->SetObjectArrayElement(t->env, elements, i, element);

      /* Fill item */
      (*t->env)->SetObjectField(t->env, element, field_elt_address,
                                newStringSafe(t->env,
                                              back[index].url_adr));
      (*t->env)->SetObjectField(t->env, element, field_elt_path,
                                newStringSafe(t->env,
                                              back[index].url_fil));
      (*t->env)->SetObjectField(t->env, element, field_elt_filename,
                                newStringSafe(t->env,
                                              back[index].url_sav));
      (*t->env)->SetBooleanField(t->env, element, field_elt_isUpdate,
                                 back[index].is_update != 0);
      (*t->env)->SetIntField(t->env, element, field_elt_state,
                             state[i].state);
      (*t->env)->SetIntField(t->env, element, field_elt_code,
                             back[index].r.statuscode);
      (*t->env)->SetObjectField(t->env, element, field_elt_message,
                                newStringSafe(t->env,
                                              back[index].r.msg));
      (*t->env)->SetBooleanField(t->env, element, field_elt_isNotModified,
                                 back[index].r.notmodified != 0);
      (*t->env)->SetBooleanField(t->env, element, field_elt_isCompressed,
                                 back[index].r.compressed!= 0);
      (*t->env)->SetLongField(t->env, element, field_elt_size,
                                 back[index].r.size);
      (*t->env)->SetLongField(t->env, element, field_elt_totalSize,
                                 back[index].r.totalsize);
      (*t->env)->SetObjectField(t->env, element, field_elt_mime,
                                newStringSafe(t->env,
                                              back[index].r.contenttype));
      (*t->env)->SetObjectField(t->env, element, field_elt_charset,
                                newStringSafe(t->env,
                                              back[index].r.charset));
    }
  }

  return ostats;
}

static int htsshow_loop_internal(jni_context_t *t, httrackp * opt,
  lien_back * back, int back_max, int back_index, int lien_n,
  int lien_tot, int stat_time, hts_stat_struct * stats) {
  int code = 1;

  /* maximum number of objects on local frame */
  const jint capacity = 128;

  /* object */
  jobject ostats;

  /* no stats ? (even loop refresh) */
  if (stats == NULL) {
    return 1;
  }

  /* no callbacks */
  assert(t != NULL);
  if (t->callbacks == NULL) {
    return 1;
  }

  /* create a new local frame for local objects (stats, strings ...) */
  if ((*t->env)->PushLocalFrame(t->env, capacity) == 0) {
    /* build stats object */
    if (stats != NULL) {
      ostats = build_stats(t, opt, back, back_max, back_index, lien_n, lien_tot,
                           stat_time, stats);
    } else {
      ostats = NULL;
    }

    /* Call refresh method */
    if (ostats != NULL || stats == NULL) {
      (*t->env)->CallVoidMethod(t->env, t->callbacks,
                                meth_HTTrackCallbacks_onRefresh, ostats);
      if ((*t->env)->ExceptionOccurred(t->env)) {
        code = 0;
      }
    } else {
      code = 0;
    }

    /* wipe local frame */
    (void) (*t->env)->PopLocalFrame(t->env, NULL);
  } else {
    return 0;  /* error */
  }

  return code;
}

static int htsshow_loop(t_hts_callbackarg * carg, httrackp * opt,
  lien_back * back, int back_max, int back_index, int lien_n,
  int lien_tot, int stat_time, hts_stat_struct * stats) {

  /* get args context */
  void *const arg = (void *) CALLBACKARG_USERDEF(carg);
  jni_context_t *const t = (jni_context_t*) arg;

  /* exit now */
  if (global_opt_stop) {
    return 0;
  }

  /* pass to internal version */
  return htsshow_loop_internal(t, opt, back, back_max, back_index,
      lien_n, lien_tot, stat_time, stats);
}

jboolean Java_com_httrack_android_jni_HTTrackLib_stop(JNIEnv* env, jobject object,
    jboolean force) {
  jboolean stopped = JNI_FALSE;
  UNUSED(env);
  UNUSED(object);
  MUTEX_LOCK(global_lock);
  if (global_opt != NULL) {
    stopped = JNI_TRUE;
    global_opt_stop = 1;
    hts_request_stop(global_opt, force);
  }
  MUTEX_UNLOCK(global_lock);
  return stopped;
}

jint Java_com_httrack_android_jni_HTTrackLib_buildTopIndex(JNIEnv* env, jclass clazz,
    jstring opath, jstring otemplates) {
  if (opath != NULL && otemplates != NULL) {
    const char* path = (*env)->GetStringUTFChars(env, opath, NULL);
    const char* templates = (*env)->GetStringUTFChars(env, otemplates, NULL);
    int ret;

    httrackp * const opt = hts_create_opt();
    opt->log = opt->errlog = NULL;
    fprintf(stderr, "building top index to %s using templates %s\n", path, templates);
    ret = hts_buildtopindex(opt, path, templates);
    hts_free_opt(opt);

    (*env)->ReleaseStringUTFChars(env, opath, path);
    (*env)->ReleaseStringUTFChars(env, otemplates, templates);

    return ret;
  } else {
    throwNPException(env, "null argument(s)");
    return -1;
  }
  UNUSED(clazz);
}

jint Java_com_httrack_android_jni_HTTrackLib_main(JNIEnv* env, jobject object,
    jobjectArray stringArray) {
  const int argc =
      object != NULL ? (*env)->GetArrayLength(env, stringArray) : 0;
  char **argv = (char**) malloc((argc + 1) * sizeof(char*));
  if (argv != NULL) {
    int i;
    httrackp * opt = NULL;
    int code = -1;
    struct jni_context_t t;
    t.env = env;
    t.callbacks = (*env)->GetObjectField(env, object, field_callbacks);

    /* Create options and reference it */
    MUTEX_LOCK(global_lock);
    if (global_opt == NULL) {
      /* Create array */
      for (i = 0; i < argc; i++) {
        jstring str = (jstring)(*env)->GetObjectArrayElement(env, stringArray,
            i);
        const char * const utf_string = (*env)->GetStringUTFChars(env, str, 0);
        argv[i] = strdup(utf_string != NULL ? utf_string : "");
        (*env)->ReleaseStringUTFChars(env, str, utf_string);
      }
      argv[i] = NULL;

      /* Create opt tab */
      opt = hts_create_opt();
      global_opt = opt;
      global_opt_stop = 0;
      CHAIN_FUNCTION(opt, loop, htsshow_loop, &t);
    }
    MUTEX_UNLOCK(global_lock);

    if (opt != NULL) {
      const hts_stat_struct* stats;

      /* Rock'in! */
      code = hts_main2(argc, argv, opt);

      /* Fetch last stats before cleaning up */
      stats = hts_get_stats(opt);
      assert(stats != NULL);
      fprintf(stderr, "status code %d, %d errors, %d warnings\n",
          code, stats->stat_errors, stats->stat_warnings);
      (void) htsshow_loop_internal(&t, opt, NULL, 0, -1, 0, 0, 0,
          (hts_stat_struct*) stats);

      /* Raise error if suitable */
      if (code == -1) {
        const char *message = hts_errmsg(opt);
        if (message != NULL && *message != '\0') {
          throwIOException(env, message);
        }
      }

      /* Unreference global option tab */
      MUTEX_LOCK(global_lock);
      hts_free_opt(opt);
      global_opt = NULL;
      global_opt_stop = 0;
      MUTEX_UNLOCK(global_lock);

      /* Cleanup */
      for (i = 0; i < argc; i++) {
        free(argv[i]);
      }
      free(argv);
    } else {
      throwRuntimeException(env, "not enough memory");
    }

    /* Return exit code. */
    return code;
  } else {
    throwRuntimeException(env, "not enough memory");
    return -1;
  }
}
