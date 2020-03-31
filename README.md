# SystemProgrammingTermProject

- - -

## nrf52832 + st7586s 를 이용해 만든 게임

- - -

## 플레이 영상

- - -

### Issue

* Interrupt를 처리하는 데에 시간을 많이 사용했다.
  * Timer Interrupt와 IO장치의 Interrupt를 동시에 Interrupt 큐에 담았을 때 IO Interrupt는 Event가 발생하지 않았음.
    * 해결을 위해 Interrupt Event Handler를 전면 수정하게 됨.
  
* Interrupt의 문제를 해결하고 난 뒤 IO장치의 Interrupt가 들어오면 Timer가 멈추는 현상 발생.



