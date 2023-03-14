main : main.c reactor.c ./ThreadPool/threadpool.c
	gcc $^ -o main

clean:
	rm main -f
