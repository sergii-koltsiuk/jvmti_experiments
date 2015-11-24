public class Test
{
    public static void main(String[] args) 
    {
        System.out.println("Hello, JVM TI");
        TestThread thread = new TestThread();
        thread.start();
        thread.stop();
        thread.stop();
    }
}