public class TestThread implements Runnable
{
	private boolean started = false;
	private Thread thread = null;

	public void start()
	{
		System.out.println("Thread start");
		started = true;
		thread = new Thread(this);
		thread.start();
	}

	public void stop() throws InterruptedException
	{
		if(!started)
		{
			System.out.println("Thread already stopped");
			return;
		}

		thread.join();
		System.out.println("Thread stopped");
		started = false;
	}

	public void run()
	{
		System.out.format("Thread begin %s\r\n", Thread.currentThread().toString());
		try
		{
	        Thread.sleep(1000);
	    }
	    catch(InterruptedException exception)
	    {
	    	System.out.format("Exception in thread end %d\r\n", Thread.currentThread());	
	    }
        System.out.format("Thread end %s\r\n", Thread.currentThread().toString());
    }
}