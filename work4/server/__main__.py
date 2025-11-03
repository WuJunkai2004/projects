import server.vercel as vercel
import server.verapi as verapi

if __name__ == "__main__":
    vercel.start(
        HandlerClass=verapi.handler,
        port=15444
    )
