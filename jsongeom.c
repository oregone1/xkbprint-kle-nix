#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define XK_TECHNICAL
#define XK_PUBLISHING
#define XK_KATAKANA
#include <stdio.h>
#include <ctype.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <X11/extensions/XKM.h>
#include <X11/extensions/XKBfile.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include <xkbcommon/xkbcommon.h>
#include <json-c/json.h>

#if defined(sgi)
#include <malloc.h>
#endif

#include <stdlib.h>

#include "utils.h"
#include "xkbprint.h"
#include "isokeys.h"

typedef struct {
    Display *           dpy;
    XkbDescPtr          xkb;
    XkbGeometryPtr      geom;
    int                 totalKB;
    int                 kbPerPage;
    int                 black;
    int                 white;
    int                 color;
    int                 font;
    int                 fontSize;
    int                 nPages;
    int                 x1, y1;
    int                 x2, y2;
    XKBPrintArgs *      args;
} PSState;

#define NLABELS     5
#define LABEL_LEN   30

typedef struct {
    unsigned    present;
    Bool        alpha[2];
    char        label[NLABELS][LABEL_LEN];
    int         font[NLABELS];
    int         size[NLABELS];
} KeyTop;

// PSShapDef()

// PSProlog() contains some useful calculations on overall size maybe

// PSDoodad() almost certainly is important to look at

// ignore PSKeycapsSymbol()

// PSNonLatin1Symbol() might be mildly interesting

static KeySym
CheckSymbolAlias(KeySym sym, PSState *state)
{
    if (XkbKSIsKeypad(sym)) {
        if ((sym >= XK_KP_0) && (sym <= XK_KP_9))
            sym = (sym - XK_KP_0) + XK_0;
        else
            switch (sym) {
            case XK_KP_Space:
                return XK_space;
            case XK_KP_Tab:
                return XK_Tab;
            case XK_KP_Enter:
                return XK_Return;
            case XK_KP_F1:
                return XK_F1;
            case XK_KP_F2:
                return XK_F2;
            case XK_KP_F3:
                return XK_F3;
            case XK_KP_F4:
                return XK_F4;
            case XK_KP_Home:
                return XK_Home;
            case XK_KP_Left:
                return XK_Left;
            case XK_KP_Up:
                return XK_Up;
            case XK_KP_Right:
                return XK_Right;
            case XK_KP_Down:
                return XK_Down;
            case XK_KP_Page_Up:
                return XK_Page_Up;
            case XK_KP_Page_Down:
                return XK_Page_Down;
            case XK_KP_End:
                return XK_End;
            case XK_KP_Begin:
                return XK_Begin;
            case XK_KP_Insert:
                return XK_Insert;
            case XK_KP_Delete:
                return XK_Delete;
            case XK_KP_Equal:
                return XK_equal;
            case XK_KP_Multiply:
                return XK_asterisk;
            case XK_KP_Add:
                return XK_plus;
            case XK_KP_Subtract:
                return XK_minus;
            case XK_KP_Divide:
                return XK_slash;
            }
    }
    else if (XkbKSIsDeadKey(sym)) {
        switch (sym) {
        case XK_dead_grave:
            sym = XK_grave;
            break;
        case XK_dead_acute:
            sym = XK_acute;
            break;
        case XK_dead_circumflex:
            sym = XK_asciicircum;
            break;
        case XK_dead_tilde:
            sym = XK_asciitilde;
            break;
        case XK_dead_macron:
            sym = XK_macron;
            break;
        case XK_dead_breve:
            sym = XK_breve;
            break;
        case XK_dead_abovedot:
            sym = XK_abovedot;
            break;
        case XK_dead_diaeresis:
            sym = XK_diaeresis;
            break;
        case XK_dead_abovering:
            sym = XK_degree;
            break;
        case XK_dead_doubleacute:
            sym = XK_doubleacute;
            break;
        case XK_dead_caron:
            sym = XK_caron;
            break;
        case XK_dead_cedilla:
            sym = XK_cedilla;
            break;
        case XK_dead_ogonek:
            sym = XK_ogonek;
            break;
        case XK_dead_iota:
            sym = XK_Greek_iota;
            break;
        case XK_dead_voiced_sound:
            sym = XK_voicedsound;
            break;
        case XK_dead_semivoiced_sound:
            sym = XK_semivoicedsound;
            break;
        }
    }
    return sym;
}

static Bool
FindKeysymsByName(XkbDescPtr xkb, char *name, PSState *state, KeyTop *top)
{
    static unsigned char buf[30];
    int kc;
    KeySym sym, *syms, topSyms[NLABELS];
    int level, group;
    int eG, nG, gI, l, g;

    bzero(top, sizeof(KeyTop));
    kc = XkbFindKeycodeByName(xkb, name, True);
    if (state->args != NULL) {
        level = state->args->labelLevel;
        group = state->args->baseLabelGroup;
    }
    else
        level = group = 0;
    syms = XkbKeySymsPtr(xkb, kc);
    eG = group;
    nG = XkbKeyNumGroups(xkb, kc);
    gI = XkbKeyGroupInfo(xkb, kc);
    if ((state->args->wantDiffs) && (eG >= XkbKeyNumGroups(xkb, kc)))
        return False;           /* XXX was a return with no value */
    if (nG == 0) {
        return False;
    }
    else if (nG == 1) {
        eG = 0;
    }
    else if (eG >= XkbKeyNumGroups(xkb, kc)) {
        switch (XkbOutOfRangeGroupAction(gI)) {
        default:
            eG %= nG;
            break;
        case XkbClampIntoRange:
            eG = nG - 1;
            break;
        case XkbRedirectIntoRange:
            eG = XkbOutOfRangeGroupNumber(gI);
            if (eG >= nG)
                eG = 0;
            break;
        }
    }
    for (g = 0; g < state->args->nLabelGroups; g++) {
        if ((eG + g) >= nG)
            continue;
        for (l = 0; l < 2; l++) {
            int font, sz;
            unsigned char utf8[30];

            if (level + l >= XkbKeyGroupWidth(xkb, kc, (eG + g)))
                continue;
            sym = syms[((eG + g) * XkbKeyGroupsWidth(xkb, kc)) + (level + l)];

            if (state->args->wantSymbols != NO_SYMBOLS)
                sym = CheckSymbolAlias(sym, state);
            topSyms[(g * 2) + l] = sym;

            utf8[0] = '\0';
            xkb_keysym_to_utf8(sym, utf8, 30);
            if ((utf8[0] >= '\x20') && (utf8[0] != '\x7f')) {
            	fprintf(stderr, "Keycode text: \"%s\"\n", utf8);
           	}

/*            if (PSKeycapsSymbol(sym, buf, &font, &sz, state)) {
                top->font[(g * 2) + l] = font;
                top->size[(g * 2) + l] = sz;
            }
            else*/ if (((sym & (~0xffUL)) == 0) && isprint(sym) && (!isspace(sym))) {
                if (sym == '(')
                    snprintf((char *) buf, sizeof(buf), "\\(");
                else if (sym == ')')
                    snprintf((char *) buf, sizeof(buf), "\\)");
                else if (sym == '\\')
                    snprintf((char *) buf, sizeof(buf), "\\\\");
                else
                    snprintf((char *) buf, sizeof(buf), "%c", (char) sym);
//                top->font[(g * 2) + l] = FONT_LATIN1;
//                top->size[(g * 2) + l] = SZ_MEDIUM;
                switch (buf[0]) {
                case '.':
                case ':':
                case ',':
                case ';':
                case '\'':
                case '"':
                case '`':
                case '~':
                case '^':
                case 0250:
                case 0270:
                case 0267:
                case 0260:
                case 0252:
                case 0272:
                case 0271:
                case 0262:
                case 0263:
                case 0264:
                case 0255:
                case 0254:
                case 0257:
//                    top->size[(g * 2) + l] = SZ_LARGE;
                    break;
                }
            }
/*			else if (PSNonLatin1Symbol(sym, buf, &font, &sz, state)) {
                top->font[(g * 2) + l] = font;
                top->size[(g * 2) + l] = sz;
			}
*/			else {
                char *tmp;

                tmp = XKeysymToString(sym);
                if (tmp != NULL)
                    strcpy((char *) buf, tmp);
                else
                    snprintf((char *) buf, sizeof(buf), "(%ld)", sym);
/*				top->font[(g * 2) + l] = FONT_LATIN1;
                if (strlen((char *) buf) < 9)
                    top->size[(g * 2) + l] = SZ_SMALL;
                else
                    top->size[(g * 2) + l] = SZ_TINY;
*/			}
            top->present |= (1 << ((g * 2) + l));
            strncpy(top->label[(g * 2) + l], (char *) buf, LABEL_LEN - 1);
            top->label[(g * 2) + l][LABEL_LEN - 1] = '\0';
        }
/*		if (((g == 0) && (top->present & G1LX_MASK) == G1LX_MASK) ||
            ((g == 1) && (top->present & G2LX_MASK) == G2LX_MASK)) {		// if all positions per layer have characters
            KeySym lower, upper;

            XConvertCase(topSyms[(g * 2)], &lower, &upper);
            if ((topSyms[(g * 2)] == lower) && (topSyms[(g * 2) + 1] == upper)) {
                top->alpha[g] = True;
            }
        }
*/	}
    return True;
}

// PSDrawLabel(FILE *out, const char *label, int x, int y, int w, int h) just for the parameters

// piece of code to go with
/*
            PSDrawLabel(out, top->label[i], col_x[sym_col[i]],
                        row_y[sym_row[i]], col_w[sym_col[i]],
                        row_h[sym_row[i]]);
        }
    }
    if (state->args->wantKeycodes) {
        char keycode[10];

        snprintf(keycode, sizeof(keycode), "%d", kc);
        PSSetFont(out, state, FONT_LATIN1, 8, True);
        PSDrawLabel(out, keycode, x + bounds->x1, y + btm - 5, w, 0);
    }
*/

// I think this is important?
static void
PSSection(FILE *out, PSState *state, XkbSectionPtr section)
{
    int r, offset;
    XkbRowPtr row;
    Display *dpy;
    XkbDescPtr xkb;

    xkb = state->xkb;
    dpy = xkb->dpy;
    fprintf(out, "%% Begin Section '%s'\n", (section->name != None ?
             XkbAtomGetString(dpy, section-> name) : "NoName"));
//	PSGSave(out, state);
    fprintf(out, "%d %d translate\n", section->left, section->top);
    if (section->angle != 0)
        fprintf(out, "%s rotate\n", XkbGeomFPText(section->angle, XkbMessage));
    if (section->doodads) {
        XkbDrawablePtr first, draw;

        first = draw = XkbGetOrderedDrawables(NULL, section);
        while (draw) {
            if (draw->type == XkbDW_Section)
                PSSection(out, state, draw->u.section);
            else
//	            PSDoodad(out, state, draw->u.doodad); // prefered, but not essential
            draw = draw->next;
        }
        XkbFreeOrderedDrawables(first);
    }
    for (r = 0, row = section->rows; r < section->num_rows; r++, row++) {
        int k;
        XkbKeyPtr key;
        XkbShapePtr shape;

        if (row->vertical)
            offset = row->top;
        else
            offset = row->left;
        fprintf(out, "%% Begin %s %d\n", row->vertical ? "column" : "row",
                r + 1);
        for (k = 0, key = row->keys; k < row->num_keys; k++, key++) {
            shape = XkbKeyShape(xkb->geom, key);
            offset += key->gap;
            if (row->vertical) {
                if (state->args->wantColor) {
                    if (key->color_ndx != state->white) {
//                      PSSetColor(out, state, key->color_ndx);
                        fprintf(out, "true 0 %d %d %s %% %s\n",
                                row->left, offset,
                                XkbAtomGetString(dpy, shape->name),
                                XkbKeyNameText(key->name.name, XkbMessage));
                    }
//                  PSSetColor(out, state, state->black);
                }
                fprintf(out, "false 0 %d %d %s %% %s\n", row->left, offset,
                        XkbAtomGetString(dpy, shape->name),
                        XkbKeyNameText(key->name.name, XkbMessage));
                offset += shape->bounds.y2;
            }
            else {
                if (state->args->wantColor) {
                    if (key->color_ndx != state->white) {
//                      PSSetColor(out, state, key->color_ndx);
                        fprintf(out, "true 0 %d %d %s %% %s\n", offset,
                                row->top, XkbAtomGetString(dpy, shape->name),
                                XkbKeyNameText(key->name.name, XkbMessage));
                    }
//                  PSSetColor(out, state, state->black);
                }
                fprintf(out, "false 0 %d %d %s %% %s\n", offset, row->top,
                        XkbAtomGetString(dpy, shape->name),
                        XkbKeyNameText(key->name.name, XkbMessage));
                offset += shape->bounds.x2;
            }
        }
    }
    for (r = 0, row = section->rows; r < section->num_rows; r++, row++) {
        int k, kc = 0;
        XkbKeyPtr key;
        XkbShapePtr shape;
        XkbBoundsRec bounds;

        if (state->args->label == LABEL_NONE)
            break;
        if (row->vertical)
            offset = row->top;
        else
            offset = row->left;
        fprintf(out, "%% Begin %s %d labels\n",
                row->vertical ? "column" : "row", r + 1);
//	    PSSetColor(out, state, xkb->geom->label_color->pixel);
//	    PSSetFont(out, state, FONT_LATIN1, 12, True);
        for (k = 0, key = row->keys; k < row->num_keys; k++, key++) {
            char *name, *name2, buf[30], buf2[30];
            int x, y;
            KeyTop top;

            shape = XkbKeyShape(xkb->geom, key);
            XkbComputeShapeTop(shape, &bounds);
            offset += key->gap;
            name = name2 = NULL;
            if (state->args->label == LABEL_SYMBOLS) {
                if (!FindKeysymsByName(xkb, key->name.name, state, &top)) {
                    fprintf(out, "%% No label for %s\n",
                            XkbKeyNameText(key->name.name, XkbMessage));
                }
            }
            else {
                char *olKey;

                if (section->num_overlays > 0)
                    olKey = XkbFindOverlayForKey(xkb->geom, section,
                                                 key->name.name);
                else
                    olKey = NULL;

                if (state->args->label == LABEL_KEYNAME) {
                    name = XkbKeyNameText(key->name.name, XkbMessage);
                    if (olKey)
                        name2 = XkbKeyNameText(olKey, XkbMessage);
                }
                else if (state->args->label == LABEL_KEYCODE) {
                    name = buf;
                    snprintf(name, sizeof(buf), "%d",
                            XkbFindKeycodeByName(xkb, key->name.name, True));
                    if (olKey) {
                        name2 = buf2;
                        snprintf(name2, sizeof(buf2), "%d",
                                 XkbFindKeycodeByName(xkb, olKey, True));
                    }
                }
                bzero(&top, sizeof(KeyTop));
                if (name2 != NULL) {
/*	                top.present |= G1LX_MASK;
                    strncpy(top.label[G1L1], name, LABEL_LEN - 1);
                    top.label[G1L1][LABEL_LEN - 1] = '\0';
                    strncpy(top.label[G1L2], name2, LABEL_LEN - 1);
                    top.label[G1L2][LABEL_LEN - 1] = '\0';
*/	            }
                else if (name != NULL) {
/*	                top.present |= CENTER_MASK;
                    strncpy(top.label[CENTER], name, LABEL_LEN - 1);
                    top.label[CENTER][LABEL_LEN - 1] = '\0';
*/	            }
                else {
                    fprintf(out, "%% No label for %s\n",
                            XkbKeyNameText(key->name.name, XkbMessage));
                }
            }
            if (row->vertical) {
                x = row->left;
                y = offset;
                offset += shape->bounds.y2;
            }
            else {
                x = offset;
                y = row->top;
                offset += shape->bounds.x2;
            }
            name = key->name.name;
            fprintf(out, "%% %s\n", XkbKeyNameText(name, XkbMessage));
            if (state->args->wantKeycodes)
                kc = XkbFindKeycodeByName(xkb, key->name.name, True);
//	        PSLabelKey(out, state, &top, x, y, &bounds, kc, shape->bounds.y2);
        }
    }
//	PSGRestore(out, state);
    return;
}

Bool
GeometryToJSON(FILE *out, XkbFileInfo *pResult, XKBPrintArgs *args)
{
    XkbDrawablePtr first, draw;
    PSState state;
    Bool dfltBorder;
    int i;

    if ((!pResult) || (!pResult->xkb) || (!pResult->xkb->geom))
        return False;
    state.xkb = pResult->xkb;
    state.dpy = pResult->xkb->dpy;
    state.geom = pResult->xkb->geom;
    state.color = state.black = state.white = -1;
    state.font = -1;
    state.nPages = 0;
    state.totalKB = 1;
    state.kbPerPage = 1;
    state.x1 = state.y1 = state.x2 = state.y2 = 0;
    state.args = args;

    if ((args->label == LABEL_SYMBOLS) && (pResult->xkb->ctrls)) {
        if (args->nTotalGroups == 0)
            state.totalKB =
                pResult->xkb->ctrls->num_groups / args->nLabelGroups;
        else
            state.totalKB = args->nTotalGroups;
        if (state.totalKB < 1)
            state.totalKB = 1;
        else if (state.totalKB > 1)
            state.kbPerPage = 2;
    }
    if (args->nKBPerPage != 0)
        state.kbPerPage = args->nKBPerPage;

//    PSProlog(out, &state);
    first = XkbGetOrderedDrawables(state.geom, NULL);

    for (draw = first, dfltBorder = True; draw != NULL; draw = draw->next) {
        if ((draw->type != XkbDW_Section) &&
            ((draw->u.doodad->any.type == XkbOutlineDoodad) ||
             (draw->u.doodad->any.type == XkbSolidDoodad))) {
            char *name;

            name = XkbAtomGetString(state.dpy, draw->u.doodad->any.name);
            if ((name != NULL) && (uStrCaseEqual(name, "edges"))) {
                dfltBorder = False;
                break;
            }
        }
    }
    for (i = 0; i < state.totalKB; i++) {
//	    PSPageSetup(out, &state, dfltBorder);
        for (draw = first; draw != NULL; draw = draw->next) {
            if (draw->type == XkbDW_Section)
                PSSection(out, &state, draw->u.section);
            else {
//	            PSDoodad(out, &state, draw->u.doodad); // temp disabled
            }
        }
//	    PSPageTrailer(out, &state);
        state.args->baseLabelGroup += state.args->nLabelGroups;
    }
    XkbFreeOrderedDrawables(first);
//	PSFileTrailer(out, &state);
    return True;
}
