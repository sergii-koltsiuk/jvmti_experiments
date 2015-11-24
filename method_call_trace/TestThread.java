public class TestThread
{
	private boolean started = false;

	public void start()
	{
		System.out.println("Thread start");
		started = true;
	}

	public void stop()
	{
		if(!started)
		{
			System.out.println("Thread already stopped");
			return;
		}

		System.out.println("Thread stopped");
		started = false;
	}
}