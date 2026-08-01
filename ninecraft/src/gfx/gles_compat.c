#include <ninecraft/gfx/gles_compat.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ninecraft_preprocessor_state {
    NINECRAFT_PREPROCESSOR_NONE,
    NINECRAFT_PREPROCESSOR_DIRECTIVE,
    NINECRAFT_PREPROCESSOR_DEFINE_NAME,
    NINECRAFT_PREPROCESSOR_DEFINE_BODY,
    NINECRAFT_PREPROCESSOR_OTHER
};

/*
 * openGL ES => openGL转译层
 *
 * 这里面绝大多数代码都是为了兼容没有GL_ARB_ES2_compatibility
 * 但是支持opengl2.0的老显卡
 */

static int ninecraft_is_identifier_start(char value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           value == '_';
}

static int ninecraft_is_identifier_part(char value) {
    return ninecraft_is_identifier_start(value) ||
           (value >= '0' && value <= '9');
}

static int ninecraft_token_equals(
    const char *source,
    size_t start,
    size_t end,
    const char *expected) {
    size_t expected_length = strlen(expected);
    return end - start == expected_length &&
           memcmp(source + start, expected, expected_length) == 0;
}

static int ninecraft_is_precision_qualifier(
    const char *source,
    size_t start,
    size_t end) {
    return ninecraft_token_equals(source, start, end, "lowp") ||
           ninecraft_token_equals(source, start, end, "mediump") ||
           ninecraft_token_equals(source, start, end, "highp");
}

static void ninecraft_blank_shader_range(
    char *source,
    size_t start,
    size_t end) {
    size_t index;
    for (index = start; index < end; index++) {
        if (source[index] != '\r' && source[index] != '\n') {
            source[index] = ' ';
        }
    }
}

static size_t ninecraft_find_shader_statement_end(
    const char *source,
    size_t length,
    size_t start) {
    size_t index = start;
    int line_comment = 0;
    int block_comment = 0;
    char quote = '\0';

    while (index < length) {
        char value = source[index];
        char next = index + 1 < length ? source[index + 1] : '\0';

        if (line_comment) {
            if (value == '\n') {
                line_comment = 0;
            }
            index++;
            continue;
        }
        if (block_comment) {
            if (value == '*' && next == '/') {
                block_comment = 0;
                index += 2;
            } else {
                index++;
            }
            continue;
        }
        if (quote != '\0') {
            if (value == '\\' && index + 1 < length) {
                index += 2;
            } else {
                if (value == quote) {
                    quote = '\0';
                }
                index++;
            }
            continue;
        }
        if (value == '/' && next == '/') {
            line_comment = 1;
            index += 2;
            continue;
        }
        if (value == '/' && next == '*') {
            block_comment = 1;
            index += 2;
            continue;
        }
        if (value == '\'' || value == '"') {
            quote = value;
            index++;
            continue;
        }
        if (value == ';') {
            return index;
        }
        index++;
    }
    return length;
}

static int ninecraft_replace_gles_100_version(
    char *source,
    size_t length) {
    size_t line_start = 0;
    int block_comment = 0;

    while (line_start < length) {
        size_t line_end = line_start;
        size_t cursor;

        while (line_end < length && source[line_end] != '\n') {
            line_end++;
        }
        cursor = line_start;
        while (cursor < line_end) {
            while (cursor < line_end &&
                   (source[cursor] == ' ' || source[cursor] == '\t' ||
                    source[cursor] == '\r')) {
                cursor++;
            }
            if (block_comment) {
                while (cursor + 1 < line_end &&
                       !(source[cursor] == '*' &&
                         source[cursor + 1] == '/')) {
                    cursor++;
                }
                if (cursor + 1 >= line_end) {
                    cursor = line_end;
                    break;
                }
                block_comment = 0;
                cursor += 2;
                continue;
            }
            if (cursor + 1 < line_end &&
                source[cursor] == '/' && source[cursor + 1] == '*') {
                block_comment = 1;
                cursor += 2;
                continue;
            }
            if (cursor + 1 < line_end &&
                source[cursor] == '/' && source[cursor + 1] == '/') {
                cursor = line_end;
            }
            break;
        }
        if (cursor < line_end && source[cursor] == '#') {
            cursor++;
            while (cursor < line_end &&
                   (source[cursor] == ' ' || source[cursor] == '\t')) {
                cursor++;
            }
            if (cursor + 7 <= line_end &&
                memcmp(source + cursor, "version", 7) == 0 &&
                (cursor + 7 == line_end ||
                 !ninecraft_is_identifier_part(source[cursor + 7]))) {
                cursor += 7;
                while (cursor < line_end &&
                       (source[cursor] == ' ' || source[cursor] == '\t')) {
                    cursor++;
                }
                if (cursor + 3 <= line_end &&
                    memcmp(source + cursor, "100", 3) == 0 &&
                    (cursor + 3 == line_end ||
                     !ninecraft_is_identifier_part(source[cursor + 3]))) {
                    source[cursor + 1] = '2';
                    return 1;
                }
            }
        }
        line_start = line_end < length ? line_end + 1 : length;
    }
    return 0;
}

static int ninecraft_strip_gles_precision_syntax(
    char *source,
    size_t length) {
    size_t index = 0;
    size_t preprocessor_start = 0;
    int changed = 0;
    int line_comment = 0;
    int block_comment = 0;
    int at_line_start = 1;
    char quote = '\0';
    enum ninecraft_preprocessor_state preprocessor =
        NINECRAFT_PREPROCESSOR_NONE;

    while (index < length) {
        char value = source[index];
        char next = index + 1 < length ? source[index + 1] : '\0';

        if (line_comment) {
            if (value == '\n') {
                line_comment = 0;
                at_line_start = 1;
                preprocessor = NINECRAFT_PREPROCESSOR_NONE;
            }
            index++;
            continue;
        }
        if (block_comment) {
            if (value == '*' && next == '/') {
                block_comment = 0;
                index += 2;
            } else {
                if (value == '\n') {
                    size_t previous = index;
                    int continued;
                    while (previous > 0 && source[previous - 1] == '\r') {
                        previous--;
                    }
                    continued = previous > 0 &&
                                source[previous - 1] == '\\';
                    at_line_start = continued ? 0 : 1;
                    if (!continued) {
                        preprocessor = NINECRAFT_PREPROCESSOR_NONE;
                    }
                }
                index++;
            }
            continue;
        }
        if (quote != '\0') {
            if (value == '\\' && index + 1 < length) {
                index += 2;
            } else {
                if (value == quote) {
                    quote = '\0';
                }
                index++;
            }
            continue;
        }
        if (value == '/' && next == '/') {
            line_comment = 1;
            index += 2;
            continue;
        }
        if (value == '/' && next == '*') {
            block_comment = 1;
            index += 2;
            continue;
        }
        if (value == '\'' || value == '"') {
            quote = value;
            at_line_start = 0;
            index++;
            continue;
        }
        if (value == '\n') {
            size_t previous = index;
            int continued;
            while (previous > 0 && source[previous - 1] == '\r') {
                previous--;
            }
            continued = previous > 0 && source[previous - 1] == '\\';
            at_line_start = continued ? 0 : 1;
            if (!continued) {
                preprocessor = NINECRAFT_PREPROCESSOR_NONE;
            }
            index++;
            continue;
        }
        if (at_line_start &&
            (value == ' ' || value == '\t' || value == '\r')) {
            index++;
            continue;
        }
        if (at_line_start && value == '#') {
            preprocessor_start = index;
            preprocessor = NINECRAFT_PREPROCESSOR_DIRECTIVE;
            at_line_start = 0;
            index++;
            continue;
        }
        if (ninecraft_is_identifier_start(value)) {
            size_t token_start = index;
            size_t token_end;

            index++;
            while (index < length &&
                   ninecraft_is_identifier_part(source[index])) {
                index++;
            }
            token_end = index;
            at_line_start = 0;

            if (preprocessor == NINECRAFT_PREPROCESSOR_DIRECTIVE) {
                preprocessor = ninecraft_token_equals(
                                   source, token_start, token_end, "define")
                                   ? NINECRAFT_PREPROCESSOR_DEFINE_NAME
                                   : NINECRAFT_PREPROCESSOR_OTHER;
                continue;
            }
            if (preprocessor == NINECRAFT_PREPROCESSOR_DEFINE_NAME) {
                if (ninecraft_is_precision_qualifier(
                        source, token_start, token_end)) {
                    size_t line_end = token_end;
                    while (line_end < length && source[line_end] != '\n') {
                        line_end++;
                    }
                    ninecraft_blank_shader_range(
                        source, preprocessor_start, line_end);
                    changed = 1;
                    index = line_end;
                    preprocessor = NINECRAFT_PREPROCESSOR_OTHER;
                } else {
                    preprocessor = NINECRAFT_PREPROCESSOR_DEFINE_BODY;
                }
                continue;
            }
            if (preprocessor == NINECRAFT_PREPROCESSOR_OTHER) {
                continue;
            }
            if (preprocessor == NINECRAFT_PREPROCESSOR_NONE &&
                ninecraft_token_equals(
                    source, token_start, token_end, "precision")) {
                size_t statement_end = ninecraft_find_shader_statement_end(
                    source, length, token_end);
                if (statement_end < length) {
                    statement_end++;
                } else {
                    statement_end = token_end;
                }
                ninecraft_blank_shader_range(
                    source, token_start, statement_end);
                changed = 1;
                index = statement_end;
                continue;
            }
            if (ninecraft_is_precision_qualifier(
                    source, token_start, token_end)) {
                ninecraft_blank_shader_range(
                    source, token_start, token_end);
                changed = 1;
            }
            continue;
        }
        if (value != ' ' && value != '\t' && value != '\r') {
            at_line_start = 0;
        }
        index++;
    }
    return changed;
}

static int ninecraft_version_string_at_least(
    const char *version,
    long required_major,
    long required_minor) {
    char *minor_start;
    long major;
    long minor;

    if (!version) {
        return 0;
    }
    while (*version != '\0' && (*version < '0' || *version > '9')) {
        version++;
    }
    major = strtol(version, &minor_start, 10);
    if (minor_start == version || *minor_start != '.') {
        return 0;
    }
    minor = strtol(minor_start + 1, NULL, 10);
    return major > required_major ||
           (major == required_major && minor >= required_minor);
}

static int ninecraft_gl_string_version_at_least(
    GLenum name,
    long required_major,
    long required_minor) {
    if (!glad_glGetString) {
        return 0;
    }
    return ninecraft_version_string_at_least(
        (const char *)glad_glGetString(name),
        required_major,
        required_minor);
}

static int ninecraft_driver_supports_glsl_120(void) {
    return ninecraft_gl_string_version_at_least(
        GL_SHADING_LANGUAGE_VERSION, 1, 20);
}

static int ninecraft_driver_accepts_gles_100_shaders(void) {
    return GLAD_GL_ARB_ES2_compatibility ||
           ninecraft_gl_string_version_at_least(GL_VERSION, 4, 1);
}

static int ninecraft_should_translate_gles_100_shaders(void) {
    return !ninecraft_driver_accepts_gles_100_shaders() &&
           ninecraft_driver_supports_glsl_120();
}

static char *ninecraft_join_shader_source(
    GLsizei count,
    const GLchar *const *string,
    const GLint *length,
    size_t *joined_length) {
    char *joined;
    size_t total = 0;
    GLsizei index;

    if (count <= 0 || !string || !joined_length) {
        return NULL;
    }
    for (index = 0; index < count; index++) {
        size_t part_length;
        if (!string[index]) {
            return NULL;
        }
        part_length = length && length[index] >= 0
                          ? (size_t)length[index]
                          : strlen(string[index]);
        if (part_length > (size_t)INT_MAX - total) {
            return NULL;
        }
        total += part_length;
    }
    joined = (char *)malloc(total + 1);
    if (!joined) {
        return NULL;
    }
    total = 0;
    for (index = 0; index < count; index++) {
        size_t part_length = length && length[index] >= 0
                                 ? (size_t)length[index]
                                 : strlen(string[index]);
        memcpy(joined + total, string[index], part_length);
        total += part_length;
    }
    joined[total] = '\0';
    *joined_length = total;
    return joined;
}

static int ninecraft_translate_gles_100_shader(
    char *source,
    size_t length) {
    if (!ninecraft_replace_gles_100_version(source, length)) {
        return 0;
    }
    ninecraft_strip_gles_precision_syntax(source, length);
    return 1;
}

static const char *ninecraft_gl_string(GLenum name) {
    const GLubyte *value;
    if (!glad_glGetString) {
        return "<glGetString unavailable>";
    }
    value = glad_glGetString(name);
    return value ? (const char *)value : "<null>";
}

static void ninecraft_log_gl_procedure(
    const char *name,
    GLADapiproc glad_procedure,
    ninecraft_gl_proc_resolver_t resolver) {
    void *resolved = resolver ? resolver(name) : NULL;
    fprintf(
        stderr,
        "  %-30s GLAD=%p resolver=%p\n",
        name,
        (void *)glad_procedure,
        resolved);
}

static void ninecraft_log_precision_format(
    const char *stage_name,
    GLenum stage,
    const char *precision_name,
    GLenum precision_type) {
    GLint range[2] = {-1, -1};
    GLint precision = -1;
    GLenum error = GL_NO_ERROR;
    int fallback = !glad_glGetShaderPrecisionFormat;

    if (fallback) {
        gl_get_shader_precision_format(
            stage, precision_type, range, &precision);
    } else {
        glGetShaderPrecisionFormat(stage, precision_type, range, &precision);
    }
    if (!fallback && glad_glGetError) {
        error = glGetError();
    }
    fprintf(
        stderr,
        "  %s %-6s: range=[%d,%d] precision=%d glError=0x%04x%s\n",
        stage_name,
        precision_name,
        range[0],
        range[1],
        precision,
        (unsigned int)error,
        fallback ? " (compatibility fallback)" : "");
}

void ninecraft_gles_log_diagnostics(
    int glad_version,
    ninecraft_gl_proc_resolver_t resolver) {
    fprintf(stderr, "\n========== OpenGL diagnostics ==========\n");
    fprintf(
        stderr,
        "GLAD load result: %d (OpenGL %d.%d)\n",
        glad_version,
        glad_version ? GLAD_VERSION_MAJOR(glad_version) : 0,
        glad_version ? GLAD_VERSION_MINOR(glad_version) : 0);
    fprintf(stderr, "GL_VENDOR: %s\n", ninecraft_gl_string(GL_VENDOR));
    fprintf(stderr, "GL_RENDERER: %s\n", ninecraft_gl_string(GL_RENDERER));
    fprintf(stderr, "GL_VERSION: %s\n", ninecraft_gl_string(GL_VERSION));
    fprintf(
        stderr,
        "GL_SHADING_LANGUAGE_VERSION: %s\n",
        ninecraft_gl_string(GL_SHADING_LANGUAGE_VERSION));
    fprintf(stderr, "GL_EXTENSIONS: %s\n", ninecraft_gl_string(GL_EXTENSIONS));
    fprintf(
        stderr,
        "GLAD flags: GL_VERSION_2_0=%d ARB_ES2_compatibility=%d "
        "ARB_framebuffer_object=%d EXT_framebuffer_object=%d\n",
        GLAD_GL_VERSION_2_0,
        GLAD_GL_ARB_ES2_compatibility,
        GLAD_GL_ARB_framebuffer_object,
        GLAD_GL_EXT_framebuffer_object);
    fprintf(
        stderr,
        "GLSL ES 1.00 handling: %s\n",
        ninecraft_driver_accepts_gles_100_shaders()
            ? "native driver path"
            : (ninecraft_driver_supports_glsl_120()
                   ? "automatic GLSL 1.20 translation"
                   : "unsupported (desktop GLSL 1.20 is required)"));

    fprintf(stderr, "Critical entry points:\n");
#define NINECRAFT_LOG_GL_PROC(name) \
    ninecraft_log_gl_procedure(      \
        #name,                       \
        (GLADapiproc)glad_##name,    \
        resolver)
    NINECRAFT_LOG_GL_PROC(glCreateShader);
    NINECRAFT_LOG_GL_PROC(glCreateProgram);
    NINECRAFT_LOG_GL_PROC(glShaderSource);
    NINECRAFT_LOG_GL_PROC(glCompileShader);
    NINECRAFT_LOG_GL_PROC(glLinkProgram);
    NINECRAFT_LOG_GL_PROC(glUseProgram);
    NINECRAFT_LOG_GL_PROC(glGetShaderPrecisionFormat);
    NINECRAFT_LOG_GL_PROC(glReleaseShaderCompiler);
    NINECRAFT_LOG_GL_PROC(glBindFramebuffer);
    NINECRAFT_LOG_GL_PROC(glBindFramebufferEXT);
    NINECRAFT_LOG_GL_PROC(glGenFramebuffers);
    NINECRAFT_LOG_GL_PROC(glGenFramebuffersEXT);
    NINECRAFT_LOG_GL_PROC(glFramebufferTexture2D);
    NINECRAFT_LOG_GL_PROC(glFramebufferTexture2DEXT);
    NINECRAFT_LOG_GL_PROC(glCheckFramebufferStatus);
    NINECRAFT_LOG_GL_PROC(glCheckFramebufferStatusEXT);
#undef NINECRAFT_LOG_GL_PROC

    fprintf(stderr, "Shader precision formats:\n");
    ninecraft_log_precision_format("vertex  ", GL_VERTEX_SHADER, "low", GL_LOW_FLOAT);
    ninecraft_log_precision_format("vertex  ", GL_VERTEX_SHADER, "medium", GL_MEDIUM_FLOAT);
    ninecraft_log_precision_format("vertex  ", GL_VERTEX_SHADER, "high", GL_HIGH_FLOAT);
    ninecraft_log_precision_format("fragment", GL_FRAGMENT_SHADER, "low", GL_LOW_FLOAT);
    ninecraft_log_precision_format("fragment", GL_FRAGMENT_SHADER, "medium", GL_MEDIUM_FLOAT);
    ninecraft_log_precision_format("fragment", GL_FRAGMENT_SHADER, "high", GL_HIGH_FLOAT);
    fprintf(stderr, "========================================\n\n");
    fflush(stderr);
}

FLOAT_ABI_FIX void gl_alpha_func(GLenum func, GLclampf ref) {
    glAlphaFunc(func, ref);
}

void gl_bind_buffer(GLenum target, GLuint buffer) {
    glBindBuffer(target, buffer);
}

void gl_bind_texture(GLenum target, GLuint texture) {
    glBindTexture(target, texture);
}

void gl_blend_func(GLenum sfactor, GLenum dfactor) {
    glBlendFunc(sfactor, dfactor);
}

void gl_blend_func_separate(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha) {
    glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
}

void gl_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    glBufferData(target, size, data, usage);
}

void gl_clear(GLbitfield mask) {
    glClear(mask);
}

FLOAT_ABI_FIX void gl_clear_color(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    glClearColor(red, green, blue, alpha);
}

FLOAT_ABI_FIX void gl_color_4_f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    glColor4f(red, green, blue, alpha);
}

void gl_color_mask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    glColorMask(red, green, blue, alpha);
}

void gl_color_pointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    glColorPointer(size, type, stride, pointer);
}

void gl_cull_face(GLenum mode) {
    glCullFace(mode);
}

void gl_delete_buffers(GLsizei n, const GLuint *buffers) {
    glDeleteBuffers(n, buffers);
}

void gl_delete_textures(GLsizei n, const GLuint *textures) {
    glDeleteTextures(n, textures);
}

void gl_depth_func(GLenum func) {
    glDepthFunc(func);
}

void gl_depth_mask(GLboolean flag) {
    glDepthMask(flag);
}

FLOAT_ABI_FIX void gl_depth_range_f(GLclampf near, GLclampf far) {
    glDepthRange((GLclampd)near, (GLclampd)far);
}

void gl_disable(GLenum cap) {
    glDisable(cap);
}

void gl_disable_client_state(GLenum array) {
    glDisableClientState(array);
}

void gl_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    glDrawArrays(mode, first, count);
}

void gl_enable(GLenum cap) {
    glEnable(cap);
}

void gl_enable_client_state(GLenum array) {
    glEnableClientState(array);
}

FLOAT_ABI_FIX void gl_fog_f(GLenum pname, GLfloat param) {
    glFogf(pname, param);
}

FLOAT_ABI_FIX void gl_fog_f_v(GLenum pname, const GLfloat *params) {
    glFogfv(pname, params);
}

void gl_fog_x(GLenum pname, GLfixed param) {
    glFogi(pname, param);
}

void gl_gen_textures(GLsizei n, GLuint *textures) {
    glGenTextures(n, textures);
}

FLOAT_ABI_FIX void gl_get_float_v(GLenum pname, GLfloat *params) {
    glGetFloatv(pname, params);
}

const GLubyte *gl_get_string(GLenum name) {
    return glGetString(name);
}

void gl_hint(GLenum target, GLenum mode) {
    glHint(target, mode);
}

FLOAT_ABI_FIX void gl_line_width(GLfloat width) {
    glLineWidth(width);
}

void gl_load_identity() {
    glLoadIdentity();
}

void gl_matrix_mode(GLenum mode) {
    glMatrixMode(mode);
}

FLOAT_ABI_FIX void gl_mult_matrix_f(const GLfloat *m) {
    glMultMatrixf(m);
}

FLOAT_ABI_FIX void gl_normal_3_f(GLfloat nx, GLfloat ny, GLfloat nz) {
    glNormal3f(nx, ny, nz);
}

FLOAT_ABI_FIX void gl_ortho_f(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far) {
    glOrtho((GLdouble)left, (GLdouble)right, (GLdouble)bottom, (GLdouble)top, (GLdouble)near, (GLdouble)far);
}

FLOAT_ABI_FIX void gl_polygon_offset(GLfloat factor, GLfloat units) {
    glPolygonOffset(factor, units);
}

void gl_pop_matrix() {
    glPopMatrix();
}

void gl_push_matrix() {
    glPushMatrix();
}

void gl_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
    glReadPixels(x, y, width, height, format, type, pixels);
}

FLOAT_ABI_FIX void gl_rotate_f(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    glRotatef(angle, x, y, z);
}

FLOAT_ABI_FIX void gl_scale_f(GLfloat x, GLfloat y, GLfloat z) {
    glScalef(x, y, z);
}

void gl_scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    glScissor(x, y, width, height);
}

void gl_shade_model(GLenum mode) {
    glShadeModel(mode);
}

void gl_tex_coord_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
    glTexCoordPointer(size, type, stride, pointer);
}

void gl_tex_image_2_d(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void gl_tex_parameter_i(GLenum target, GLenum pname, GLint param) {
    glTexParameteri(target, pname, param);
}

void gl_tex_sub_image_2_d(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) {
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

FLOAT_ABI_FIX void gl_translate_f(GLfloat x, GLfloat y, GLfloat z) {
    glTranslatef(x, y, z);
}

void gl_vertex_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
    glVertexPointer(size, type, stride, pointer);
}

void gl_viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    glViewport(x, y, width, height);
}

void gl_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    glDrawElements(mode, count, type, indices);
}

GLenum gl_get_error() {
    return glGetError();
}

void gl_gen_buffers(GLsizei n, GLuint *buffers) {
    glGenBuffers(n, buffers);
}

void gl_stencil_func(GLenum func, GLint ref, GLuint mask) {
    glStencilFunc(func, ref, mask);
}

void gl_stencil_mask(GLuint mask) {
    glStencilMask(mask);
}

FLOAT_ABI_FIX void gl_light_model_f(GLenum pname, GLfloat param) {
    glLightModelf(pname, param);
}

FLOAT_ABI_FIX void gl_light_f_v(GLenum light, GLenum pname, const GLfloat *params) {
    glLightfv(light, pname, params);
}

void gl_normal_pointer(GLenum type, GLsizei stride, const GLvoid *pointer) {
    glNormalPointer(type, stride, pointer);
}

void gl_stencil_op(GLenum fail, GLenum zfail, GLenum zpass) {
    glStencilOp(fail, zfail, zpass);
}

void gl_active_texture(GLenum texture) {
    glActiveTexture(texture);
}

void gl_attach_shader(GLuint program, GLuint shader) {
    glAttachShader(program, shader);
}

void gl_clear_stencil(GLint s) {
    glClearStencil(s);
}

void gl_compile_shader(GLuint shader) {
    glCompileShader(shader);
}

GLuint gl_create_program() {
    return glCreateProgram();
}

GLuint gl_create_shader(GLenum type) {
    return glCreateShader(type);
}

void gl_delete_program(GLuint program) {
    glDeleteProgram(program);
}

void gl_enable_vertex_attrib_array(GLuint index) {
    glEnableVertexAttribArray(index);
}

void gl_get_active_attrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void gl_get_active_uniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

GLint gl_get_attrib_location(GLuint program, const GLchar *name) {
    return glGetAttribLocation(program, name);
}

void gl_get_program_info_log(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    glGetProgramInfoLog(program, bufSize, length, infoLog);
}

void gl_get_program_i_v(GLuint program, GLenum pname, GLint *params) {
    glGetProgramiv(program, pname, params);
}

void gl_get_shader_info_log(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

void gl_get_shader_i_v(GLuint shader, GLenum pname, GLint *params) {
    glGetShaderiv(shader, pname, params);
}

void gl_get_shader_precision_format(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision) {
    if (glad_glGetShaderPrecisionFormat) {
        glad_glGetShaderPrecisionFormat(
            shadertype, precisiontype, range, precision);
        return;
    }

    (void)shadertype;
    if (precisiontype == GL_LOW_INT ||
        precisiontype == GL_MEDIUM_INT ||
        precisiontype == GL_HIGH_INT) {
        if (range) {
            range[0] = 31;
            range[1] = 30;
        }
        if (precision) {
            *precision = 0;
        }
    } else if (precisiontype == GL_LOW_FLOAT) {
        if (range) {
            range[0] = 1;
            range[1] = 1;
        }
        if (precision) {
            *precision = 8;
        }
    } else if (precisiontype == GL_MEDIUM_FLOAT) {
        if (range) {
            range[0] = 14;
            range[1] = 14;
        }
        if (precision) {
            *precision = 10;
        }
    } else if (precisiontype == GL_HIGH_FLOAT) {
        if (range) {
            range[0] = 62;
            range[1] = 62;
        }
        if (precision) {
            *precision = 16;
        }
    } else {
        if (range) {
            range[0] = 0;
            range[1] = 0;
        }
        if (precision) {
            *precision = 0;
        }
    }
}

GLint gl_get_uniform_location(GLuint program, const GLchar *name) {
    return glGetUniformLocation(program, name);
}

void gl_link_program(GLuint program) {
    glLinkProgram(program);
}

void gl_release_shader_compiler() {
    if (glad_glReleaseShaderCompiler) {
        glad_glReleaseShaderCompiler();
    }
}

void gl_shader_source(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length) {
    char *translated_source;
    size_t translated_size;

    if (!ninecraft_should_translate_gles_100_shaders()) {
        glShaderSource(shader, count, string, length);
        return;
    }

    translated_source = ninecraft_join_shader_source(
        count, string, length, &translated_size);
    if (translated_source &&
        ninecraft_translate_gles_100_shader(
            translated_source, translated_size)) {
        const GLchar *source_pointer = translated_source;
        GLint source_length = (GLint)translated_size;
        glShaderSource(shader, 1, &source_pointer, &source_length);
        free(translated_source);
        return;
    }
    free(translated_source);
    glShaderSource(shader, count, string, length);
}

FLOAT_ABI_FIX void gl_uniform_1_f_v(GLint location, GLsizei count, const GLfloat *value) {
    glUniform1fv(location, count, value);
}

void gl_uniform_1_i_v(GLint location, GLsizei count, const GLint *value) {
    glUniform1iv(location, count, value);
}

FLOAT_ABI_FIX void gl_uniform_2_f_v(GLint location, GLsizei count, const GLfloat *value) {
    glUniform2fv(location, count, value);
}

void gl_uniform_2_i_v(GLint location, GLsizei count, const GLint *value) {
    glUniform2iv(location, count, value);
}

FLOAT_ABI_FIX void gl_uniform_3_f_v(GLint location, GLsizei count, const GLfloat *value) {
    glUniform3fv(location, count, value);
}

void gl_uniform_3_i_v(GLint location, GLsizei count, const GLint *value) {
    glUniform3iv(location, count, value);
}

FLOAT_ABI_FIX void gl_uniform_4_f_v(GLint location, GLsizei count, const GLfloat *value) {
    glUniform4fv(location, count, value);
}

void gl_uniform_4_i_v(GLint location, GLsizei count, const GLint *value) {
    glUniform4iv(location, count, value);
}

FLOAT_ABI_FIX void gl_uniform_matrix_2_f_v(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    glUniformMatrix2fv(location, count, transpose, value);
}

FLOAT_ABI_FIX void gl_uniform_matrix_3_f_v(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    glUniformMatrix3fv(location, count, transpose, value);
}

FLOAT_ABI_FIX void gl_uniform_matrix_4_f_v(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    glUniformMatrix4fv(location, count, transpose, value);
}

void gl_use_program(GLuint program) {
    glUseProgram(program);
}

void gl_vertex_attrib_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void gl_stencil_func_separate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    glStencilFuncSeparate(face, func, ref, mask);
}

void gl_stencil_op_separate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    glStencilOpSeparate(face, sfail, dpfail, dppass);
}

void gl_delete_shader(GLuint shader) {
    glDeleteShader(shader);
}

void gl_uniform_1_i(GLint location, GLint v0) {
    glUniform1i(location, v0);
}

void gl_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    glBufferSubData(target, offset, size, data);
}

void gl_bind_renderbuffer(GLenum target, GLuint renderbuffer) {
    if (glad_glBindRenderbuffer) {
        glad_glBindRenderbuffer(target, renderbuffer);
    } else if (glad_glBindRenderbufferEXT) {
        glad_glBindRenderbufferEXT(target, renderbuffer);
    }
}

void gl_gen_renderbuffers(GLsizei n, GLuint *renderbuffers) {
    if (glad_glGenRenderbuffers) {
        glad_glGenRenderbuffers(n, renderbuffers);
    } else if (glad_glGenRenderbuffersEXT) {
        glad_glGenRenderbuffersEXT(n, renderbuffers);
    } else if (n > 0 && renderbuffers) {
        memset(renderbuffers, 0, (size_t)n * sizeof(*renderbuffers));
    }
}

void gl_delete_renderbuffers(GLsizei n, const GLuint *renderbuffers) {
    if (glad_glDeleteRenderbuffers) {
        glad_glDeleteRenderbuffers(n, renderbuffers);
    } else if (glad_glDeleteRenderbuffersEXT) {
        glad_glDeleteRenderbuffersEXT(n, renderbuffers);
    }
}

void gl_bind_framebuffer(GLenum target, GLuint framebuffer) {
    if (glad_glBindFramebuffer) {
        glad_glBindFramebuffer(target, framebuffer);
    } else if (glad_glBindFramebufferEXT) {
        glad_glBindFramebufferEXT(target, framebuffer);
    }
}

GLenum gl_check_framebuffer_status(GLenum target) {
    if (glad_glCheckFramebufferStatus) {
        return glad_glCheckFramebufferStatus(target);
    }
    if (glad_glCheckFramebufferStatusEXT) {
        return glad_glCheckFramebufferStatusEXT(target);
    }
    return GL_FRAMEBUFFER_UNSUPPORTED;
}

void gl_gen_framebuffers(GLsizei n, GLuint *framebuffers) {
    if (glad_glGenFramebuffers) {
        glad_glGenFramebuffers(n, framebuffers);
    } else if (glad_glGenFramebuffersEXT) {
        glad_glGenFramebuffersEXT(n, framebuffers);
    } else if (n > 0 && framebuffers) {
        memset(framebuffers, 0, (size_t)n * sizeof(*framebuffers));
    }
}

void gl_delete_framebuffers(GLsizei n, const GLuint *framebuffers) {
    if (glad_glDeleteFramebuffers) {
        glad_glDeleteFramebuffers(n, framebuffers);
    } else if (glad_glDeleteFramebuffersEXT) {
        glad_glDeleteFramebuffersEXT(n, framebuffers);
    }
}

void gl_get_integer_v(GLenum pname, GLint *data) {
    GLint components;

    if (!ninecraft_driver_accepts_gles_100_shaders()) {
        switch (pname) {
            case GL_MAX_VERTEX_UNIFORM_VECTORS:
                glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &components);
                if (data) {
                    *data = components / 4;
                }
                return;
            case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
                glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &components);
                if (data) {
                    *data = components / 4;
                }
                return;
            case GL_MAX_VARYING_VECTORS:
                glGetIntegerv(GL_MAX_VARYING_FLOATS, &components);
                if (data) {
                    *data = components / 4;
                }
                return;
            case GL_SHADER_COMPILER:
                if (data) {
                    *data = GL_TRUE;
                }
                return;
            case GL_NUM_SHADER_BINARY_FORMATS:
                if (data) {
                    *data = 0;
                }
                return;
            case GL_SHADER_BINARY_FORMATS:
                return;
            case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
                if (data) {
                    *data = GL_RGBA;
                }
                return;
            case GL_IMPLEMENTATION_COLOR_READ_TYPE:
                if (data) {
                    *data = GL_UNSIGNED_BYTE;
                }
                return;
            default:
                break;
        }
    }
    glGetIntegerv(pname, data);
}

FLOAT_ABI_FIX void gl_clear_depth_f(GLclampf depth) {
    glClearDepth((GLdouble)depth);
}

void gl_framebuffer_renderbuffer(GLenum target, GLenum attachment, GLenum renderbuffer_target, GLuint renderbuffer) {
    if (glad_glFramebufferRenderbuffer) {
        glad_glFramebufferRenderbuffer(
            target, attachment, renderbuffer_target, renderbuffer);
    } else if (glad_glFramebufferRenderbufferEXT) {
        glad_glFramebufferRenderbufferEXT(
            target, attachment, renderbuffer_target, renderbuffer);
    }
}

void gl_renderbuffer_storage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!ninecraft_driver_accepts_gles_100_shaders() &&
        internalformat == GL_RGB565) {
        internalformat = GL_RGB8;
    }
    if (glad_glRenderbufferStorage) {
        glad_glRenderbufferStorage(target, internalformat, width, height);
    } else if (glad_glRenderbufferStorageEXT) {
        glad_glRenderbufferStorageEXT(target, internalformat, width, height);
    }
}

void gl_flush(void) {
    glFlush();
}

GLboolean gl_is_texture(GLuint texture) {
    return glIsTexture(texture);
}

void gl_get_tex_parameter_i_v(GLenum target, GLenum pname, GLint *params) {
    glGetTexParameteriv(target, pname, params);
}

void gl_framebuffer_texture_2_d(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    if (glad_glFramebufferTexture2D) {
        glad_glFramebufferTexture2D(
            target, attachment, textarget, texture, level);
    } else if (glad_glFramebufferTexture2DEXT) {
        glad_glFramebufferTexture2DEXT(
            target, attachment, textarget, texture, level);
    }
}
