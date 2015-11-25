public class Test
{
    public static void main(String[] args) 
    {
    	long startTime = System.currentTimeMillis();

        System.out.println("Start test");

        for (int i = 0; i<1; ++i)
        {
        	TestThread thread = new TestThread();
        	thread.start();
        	thread.stop();
        	thread.stop();
    	}

        long runTime = System.currentTimeMillis() - startTime;
        System.out.println("Test execution time: " + runTime + "ms");
    }
}