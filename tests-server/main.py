from fastapi import FastAPI
from pydantic import BaseModel

app = FastAPI()


# 受信データの型定義
class SensorData(BaseModel):
    co2_ppm: int
    temperature: int  # milli-degrees Celsius
    humidity: int  # milli-percent relative humidity


@app.post("/api/readings")
async def receive_data(data: SensorData):
    print("--- 届いたデータ ---")
    print(f"Co2濃度: {data.co2_ppm} ")
    print(f"温度: {data.temperature / 1000.0} ℃")
    print(f"湿度: {data.humidity / 1000.0} %")
    return {"status": "success", "message": "Data received"}


if __name__ == "__main__":
    import uvicorn

    # PCのIPアドレス（0.0.0.0）で待ち受け
    uvicorn.run(app, host="0.0.0.0", port=5173)
