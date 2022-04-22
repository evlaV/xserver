/*
* Copyright © 2020 Red Hat
* Copyright © 2022 David Redondo <kde@david-redondo.de>
*
* Permission to use, copy, modify, distribute, and sell this software
* and its documentation for any purpose is hereby granted without
* fee, provided that the above copyright notice appear in all copies
* and that both that copyright notice and this permission notice
* appear in supporting documentation, and that the name of the
* copyright holders not be used in advertising or publicity
* pertaining to distribution of the software without specific,
* written prior permission.  The copyright holders make no
* representations about the suitability of this software for any
* purpose.  It is provided "as is" without express or implied
* warranty.
*
* THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
* SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
* FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
* SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
* AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/

#include <xwayland-config.h>

#include <inputstr.h>
#include <inpututils.h>
#include <stdbool.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "xwayland-input.h"
#include "xwayland-screen.h"
#include "xwayland-xtest.h"

#include "fake-input-client-protocol.h"



#define log(...) ErrorF("[xwayland xtest] " __VA_ARGS__)

static struct org_kde_kwin_fake_input *fake_input = NULL;

static void
xwayland_xtest_send_events(DeviceIntPtr dev,
                        int type,
                        int detail,
                        int flags,
                        int firstValuator,
                        int numValuators,
                        const int *valuators)
{
    if (!fake_input)  {
        return;
    }

    ClientPtr client = GetCurrentClient();
    ValuatorMask mask;

    valuator_mask_zero(&mask);
    valuator_mask_set_range(&mask, firstValuator, numValuators, valuators);

    uint32_t fake_input_version = wl_proxy_get_version((struct wl_proxy *)fake_input);

    switch (type) {
        case MotionNotify: {
            int x = 0, y = 0;
            valuator_mask_fetch(&mask, 0, &x);
            valuator_mask_fetch(&mask, 1, &y);

            if ((flags & POINTER_ABSOLUTE) == 0) {
                org_kde_kwin_fake_input_pointer_motion(fake_input, wl_fixed_from_int(x), wl_fixed_from_int(y));
            } else if (fake_input_version >= ORG_KDE_KWIN_FAKE_INPUT_POINTER_MOTION_ABSOLUTE_SINCE_VERSION) {
                org_kde_kwin_fake_input_pointer_motion(fake_input, wl_fixed_from_int(x), wl_fixed_from_int(y));
            }
            break;
        }

        case ButtonPress:
        case ButtonRelease:
            if (detail < 4 || detail > 7) {
                int button = 0;
                switch (dev->button->map[detail]) {
                case 1:
                    button = BTN_LEFT;
                    break;
                case 2:
                    button = BTN_MIDDLE;
                    break;
                case 3:
                    button = BTN_RIGHT;
                    break;
                case 5:
                    button = BTN_SIDE;
                    break;
                case 8:
                    button = BTN_BACK;
                    break;
                case 9:
                    button = BTN_FORWARD;
                    break;
                }
                org_kde_kwin_fake_input_button(fake_input, button, type == ButtonPress ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
            } else if (ButtonRelease) {
                uint32_t axis = detail < 6 ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL;
                uint32_t amount = (detail == 4 || detail == 6) ? -10 : 10;
                org_kde_kwin_fake_input_axis(fake_input, axis, amount);
            }
        break;
        case KeyPress:
        case KeyRelease:
            if (fake_input_version >= ORG_KDE_KWIN_FAKE_INPUT_KEYBOARD_KEY_SINCE_VERSION) {
                org_kde_kwin_fake_input_keyboard_key(fake_input, detail - 8, type == KeyPress ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
            }
        break;
    }
}

void xwayland_override_xtest_fake_input(struct xwl_screen *xwl_screen, uint32_t id, uint32_t version)
{
    fake_input = wl_registry_bind(xwl_screen->registry, id, &org_kde_kwin_fake_input_interface, min(4, version));
    org_kde_kwin_fake_input_authenticate(fake_input, "XWayland", "XTest Events");

    struct xwl_seat *xwl_seat;
    xorg_list_for_each_entry(xwl_seat, &xwl_screen->seat_list, link) {
        // Binding fake_input can add a capability to the seat, creating a wl_seat.capabilities event that the screen doesn't expect
        if (!xwl_seat->wl_pointer || !xwl_seat->wl_keyboard || ! xwl_seat->wl_touch) {
            ++xwl_screen->expecting_event;
        }
    }

    DeviceIntPtr d;
    nt_list_for_each_entry(d, inputInfo.devices, next) {
        /* Should really have a better hook for this ... */
        if (strstr(d->name, "XTEST pointer")) {
            log("Overriding XTest for %s\n", d->name);
            d->sendEventsProc = xwayland_xtest_send_events;
        }
        if (strstr(d->name, "XTEST keyboard")) {
            log("Overriding XTest for %s\n", d->name);
            d->sendEventsProc = xwayland_xtest_send_events;
        }
    }
}
