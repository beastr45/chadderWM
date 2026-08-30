/**
 * Author: Fernando C.V.
 * http://lists.suckless.org/dev/1104/7590.html
 */

/** Function to shift the current view to the left/right
 *
 * @param: "arg->i" stores the number of tags to shift right (positive value)
 *          or left (negative value)
 */
void shiftview(const Arg *arg) {
  Arg shifted;

  if (arg->i > 0) // left circular shift
    shifted.ui =
        (sel_mon->tagset[sel_mon->sel_tags] << arg->i) |
        (sel_mon->tagset[sel_mon->sel_tags] >> (LENGTH(tags) - arg->i));

  else // right circular shift
    shifted.ui = sel_mon->tagset[sel_mon->sel_tags] >> (-arg->i) |
                 sel_mon->tagset[sel_mon->sel_tags] << (LENGTH(tags) + arg->i);

  view(&shifted);
}
