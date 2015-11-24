public class bridge 
{

    /* Master switch that activates methods. */

    private static int engaged = 0;

    /* At the very beginning of every method, a call to method_entry()
     *     is injected.
     */

    private static native void _method_entry(Object thr, int cnum, int mnum);
    public static void method_entry(int cnum, int mnum)
    {
        if ( engaged != 0 ) 
        {
            _method_entry(Thread.currentThread(), cnum, mnum);
        }
    }

    /* Before any of the return bytecodes, a call to method_exit()
     *     is injected.
     */

    private static native void _method_exit(Object thr, int cnum, int mnum);
    public static void method_exit(int cnum, int mnum)
    {
        if ( engaged != 0 ) 
        {
            _method_exit(Thread.currentThread(), cnum, mnum);
        }
    }

}
