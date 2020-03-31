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
  * 이후 Library를 찾아보니 Schedule을 다룰 수 있는 함수를 발견
    * app_sched_execute() 메소드를 이용해 Interrupt를 처리
    
* 현재까지 개발한 프로젝트 중 Line 수로만 따지면 가장 긴 프로젝트
  * 그중 90%가 UI를 구성하는 Code
    * 마친 후에 든 생각이지만 각 Object들을 struct로 구성하여 개발했으면 Code가 훨씬 줄었을 것이다.
    
### 마친 후

* H/W의 구동원리를 이해하게 됨.
* 이론으로만 알던 Event Queue를 다뤄보는 좋은 기회였다.



