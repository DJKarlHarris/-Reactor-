main : main.c reactor.c
	gcc $^ -o main

clean:
	rm main -f
