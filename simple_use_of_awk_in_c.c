#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define AWK_OK 0
#define AWK_CONTINUE 1
#define AWK_BREAK 2
#define AWK_FIELD_OUTOFRANGE 3
#define AWK_LINE_OUTOFRANGE 4
#define AWK_OPEN_FAILED 5

/*
 * BSD license.
 *
 * fun_begin, fun_line and fun_end are the function for when begin , every line and end.
 * data are user defined type for private use inside the functions, change the type to what it is in the functions.
 *
 * fun_end return none value, while the fun_begin and fun_line
 * return AWK_CONTINUE to continue, return AWK_BREAK to stop excution.
 *
 * if success awk return AWK_OK, otherwise an errno that can be reported by awk_errno is returned.
 *
 * modify line and fields to satify your use, an example is appended to the code.
 *
 * Note: the return value that the caller wanted should be stored in data.
 *       what fun_begin and func_line return are just for awk, and what awk return is only used for reporting awk errors.
 *
 * TODO: add a match function.
 */

typedef int (*awk_begin_t)(void *data);
typedef int (*awk_line_t)(int row_idx, char *fields[], int num_of_fields, void *data);
typedef void (*awk_end_t)(int row_idx, char *fields[], int num_of_fields, void *data);
int awk(const char *filename, const char *delim, awk_begin_t fun_begin, awk_line_t fun_line,  awk_end_t fun_end, void *data);
const char *awk_error(int err);



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

int awk__(FILE *stream, const char *delim, char line[], int linesize, char *fields[], int fieldnum, awk_begin_t fun_begin, awk_line_t fun_line, awk_end_t fun_end, void *data)
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
    int i, field_idx, found;

    if(fun_begin(data) != AWK_CONTINUE)
        return AWK_OK;

    errno = 0;
    while(fgets(line, linesize, stream) || errno == EINTR)
    {
        if(errno == EINTR)
            continue;

        for(i = 0,field_idx = 0, found=1; i < linesize
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

        if(fun_line(row_idx, fields, field_idx, data) != AWK_CONTINUE)
            return AWK_OK;

        row_idx++;
    }

    fun_end(row_idx, fields, field_idx, data);
    return AWK_OK;
}

int awk_(const char *filename, const char *delim, char line[], int linesize, char *fields[], int fieldnum, awk_begin_t fun_begin, awk_line_t fun_line, awk_end_t fun_end, void *data)
{
    int ret;
    ret = call_with_inputfile(filename, awk__, delim, line, linesize, fields, fieldnum, fun_begin, fun_line, fun_end, data);
    if(ret < 0)
        return AWK_OPEN_FAILED;
    return ret;
}
int awk(const char *filename, const char *delim, awk_begin_t fun_begin, awk_line_t fun_line,  awk_end_t fun_end, void *data)
{
    char line[512];
    char *fields[10];
    return awk_(filename, delim, line, sizeof line, fields, sizeof fields, fun_begin, fun_line, fun_end, data);
}








/* below is an example of how to use */
/*
struct buf_st
{
    char buf[1024];
    int i;
    int ret;
};
int func_begin(void *data)
{
    struct buf_st *buf = data;
    int i, ret;
    i = buf->i;
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
    ret = snprintf(&buf->buf[i], sizeof buf->buf - i, "\n total num: %d\n", row_idx);
    if(ret < 0)
    {
        buf->ret = ret;
        return;
    }
    buf->i += ret;
    buf->ret = 0;
}
int func_line(int row_idx, char *fields[], int num_of_fields, void *data)
{
    struct buf_st *buf = data;
    int i, ret;
    (void) num_of_fields;
    i = buf->i;
    ret = snprintf(&buf->buf[i], sizeof buf->buf - i, "\t %d. %s\n", row_idx, fields[0]);
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
    int ret = awk("/etc/passwd", ":", func_begin, func_line, func_end, &buf);
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
*/
