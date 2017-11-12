#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#define AWK_OK                  0
#define AWK_CONTINUE            1
#define AWK_BREAK               2
#define AWK_FIELD_OUTOFRANGE    3
#define AWK_LINE_OUTOFRANGE     4
#define AWK_OPEN_FAILED         5
#define AWK_REGCOMP             6

/*
 * BSD license.
 *
 * fun_begin, fun_line and fun_end are the function for when begin , every line and end. if they are unused pass NULL to the function.
 * data are user defined type for private use inside the functions, change the type to what it is in the functions.
 * a pattern string (initialized char array) must be put in the front of data, if pattern is not used, use ""
 * if the user defined type doesn't reserve the space for pattern and give it a proper value, the function may crash. carefully use.
 * if fields[0] is set to AWK_FIELD0_USED or delim is an empty string, $0 will be saved in fields[0], otherwise $0 will be an empty string.
 * all unused fields are set to empty by default.
 *
 * fun_end return none value, while the fun_begin and fun_line
 * return AWK_CONTINUE to continue, return AWK_BREAK to stop excution.
 * fun_end is always excuted even when meets AWK_BREAK.
 *
 * if success awk return AWK_OK, otherwise an errno that can be reported by awk_errno is returned.
 *
 * the temporary fields and line will be unavaliable if they are in an stack and when the stack is unavaliable.
 * modify line and fields to satify your use, an example is appended to the code.
 *
 * Note: the return value that the caller wanted should be stored in data.
 *       what fun_begin and func_line return are just for awk, and what awk return is only used for reporting awk errors.
 *
 */

typedef int (*awk_begin_t)(void *data);
typedef int (*awk_action_t)(int row_idx, char *fields[], int num_of_fields, void *data);
typedef void (*awk_end_t)(int row_idx, char *fields[], int num_of_fields, void *data);
int awk_(const char *filename, const char *delim, char line[], int linesize, char *fields[], int fieldnum, awk_begin_t fun_begin, awk_action_t fun_action, awk_end_t fun_end, void *data);
    /* modify it before use */
int awk(const char *filename, const char *delim, awk_begin_t fun_begin, awk_action_t fun_action,  awk_end_t fun_end, void *data);
const char *awk_error(int err);
#define AWK_FIELD0_USED (void*)-1



const char *awk_error(int err)
{
    switch(err)
    {
        case AWK_FIELD_OUTOFRANGE:
            return "fields is too small";
        case AWK_LINE_OUTOFRANGE:
            return "line is too small";
        case AWK_OPEN_FAILED:
            return "open file failed";
        case AWK_REGCOMP:
            return "regcomp failed";
        default:
            return "unknown failed";
    }
}


#define call_with_inputfile(filename, f, argv...) \
    ({\
        int ret;\
        FILE *stream;\
        stream = fopen(filename, "r");\
        if (stream == NULL) {\
            ret = -1;\
        }\
        ret = f(stream, ##argv);\
        fclose(stream);\
        ret;\
    })

int awk_match(const char *pattern, const char *line)
{
    int ret;
    regex_t preg;
    regmatch_t  pmatch[1];

    if(regcomp(&preg, pattern, 0) != 0)
        return AWK_REGCOMP;

    ret = regexec(&preg, line, 1, pmatch, 0);

    //size_t regerror(int errcode, const regex_t *preg, char *errbuf,
    //       size_t errbuf_size);

    regfree(&preg);
    return ret == 0;
}
int awk__(FILE *stream, const char *delim, char line[], int linesize, char *fields[], int fieldnum, awk_begin_t fun_begin, awk_action_t fun_action, awk_end_t fun_end, void *data)
{
#define ADD_FIELD(found) \
        if(found)\
        do{\
            if(field_idx >= fieldnum)\
                return AWK_FIELD_OUTOFRANGE;\
            fields[field_idx++]=&line[i];\
            found = 0;\
        }while(0)

    int row_idx = 0;
    int i, field_idx, found, field0_used;

    if(fieldnum < 1)                    /* at least one field */
        return AWK_FIELD_OUTOFRANGE;

    if(fun_begin)
    {
        if(fun_begin(data) != AWK_CONTINUE)
            return AWK_OK;
    }

    if(fields[0] == AWK_FIELD0_USED || *delim == 0)
        field0_used = 1;
    else
        field0_used = 0;

    for (i = 0; i < fieldnum; ++i) {
        static char *empty="";
        fields[i] = empty;
    }
    errno = 0;
    while(fgets(line, linesize, stream) || errno == EINTR)
    {
        if(!awk_match(data, line))
            continue;
        if(errno == EINTR)
        {
            errno = 0;
            continue;
        }

        for (i = 0; i < fieldnum; ++i) {
            static char *empty="";
            fields[i] = empty;
        }
        field_idx = 1; 
        if(*delim)
        {
            if(field0_used)
            {
                int l = strlen(line);
                if((l*2) > linesize-2)
                    return AWK_LINE_OUTOFRANGE;
                fields[0] = &line[l+1];
                strcpy(fields[0], &line[0]);
            }
            for(i = 0,found=1; i < linesize
                    && line[i] != '\n' && line[i] != 0; i++)
            {
                ADD_FIELD(found);
                if(strchr(delim, line[i]))
                {
                    line[i] = 0;
                    found = 1;
                }
            }
            if(i == linesize)
                return AWK_LINE_OUTOFRANGE;
            line[i] = 0; // if '\n', -> '\0'
            ADD_FIELD(found);
        }
        else
        {
            fields[0] = line;
        }

        if(fun_action)
        {
            if(fun_action(row_idx, fields, field_idx, data) != AWK_CONTINUE)
                return AWK_OK;
        }

        row_idx++;
    }

    if(fun_end)
    {
        fun_end(row_idx, fields, field_idx, data);
    }
    return AWK_OK;
}

int awk_(const char *filename, const char *delim, char line[], int linesize, char *fields[], int fieldnum, awk_begin_t fun_begin, awk_action_t fun_action, awk_end_t fun_end, void *data)
{
    int ret;
    ret = call_with_inputfile(filename, awk__, delim, line, linesize, fields, fieldnum, fun_begin, fun_action, fun_end, data);
    if(ret < 0)
        return AWK_OPEN_FAILED;
    return ret;
}
int awk(const char *filename, const char *delim, awk_begin_t fun_begin, awk_action_t fun_action,  awk_end_t fun_end, void *data)
{
    char line[5120];
    char *fields[10];
    fields[0] = AWK_FIELD0_USED;
    return awk_(filename, delim, line, sizeof line, fields, sizeof fields/sizeof fields[0], fun_begin, fun_action, fun_end, data);
}








/* below is an example of how to use
 * it will print the $1 and $0 of each line in /etc/passwd with delim : */


struct buf_st
{
    char pattern[32];
    char buf[10240];
    int i;
    int ret;
};
int func_begin(void *data)
{
    struct buf_st *buf = data;
    int i, ret;
    i = buf->i;
    if(sizeof buf->buf - i <= 0)
        return AWK_BREAK;
    ret = snprintf(&buf->buf[i], sizeof buf->buf - i, "users are: \n");
    if(ret < 0)
    {
        buf->ret = ret;
        return AWK_BREAK;
    }
    buf->i += ret;
    return AWK_CONTINUE;
}
void func_end(int row_idx, char *fields[], int num_of_fields, void *data)
{
    struct buf_st *buf = data;
    int i, ret;
    i = buf->i;
    (void)fields;
    (void)num_of_fields;
    if(sizeof buf->buf - i <= 0)
        return;
    ret = snprintf(&buf->buf[i], sizeof buf->buf - i, "\n total num: %d\n", row_idx);
    if(ret < 0)
    {
        buf->ret = ret;
        return;
    }
    buf->i += ret;
    buf->ret = 0;
}
int func_action(int row_idx, char *fields[], int num_of_fields, void *data)
{
    struct buf_st *buf = data;
    int i, ret;
    (void) num_of_fields;
    i = buf->i;
    if(sizeof buf->buf - i <= 0)
        return AWK_BREAK;
    ret = snprintf(&buf->buf[i], sizeof buf->buf - i, "\t %d. %s %s\n", row_idx, fields[1], fields[0]);
    if(ret < 0)
    {
        buf->ret = ret;
        return AWK_BREAK;
    }
    buf->i += ret;
    return AWK_CONTINUE;
}

void example(void)
{
    struct buf_st buf = {{0}};
    strcpy(buf.pattern, "d*.nal");
    int ret = awk("/etc/passwd", ":", func_begin, func_action, func_end, &buf);
    if(ret != AWK_OK)
    {
        fprintf(stderr, "awk wrong:%s\n", awk_error(ret));
    }
    fprintf(stdout, "%s", buf.buf);
}
int main(void)
{
    example();
    return 0;
}


