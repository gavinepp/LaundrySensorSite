import { Component } from '@angular/core';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.scss']
})
export class AppComponent {
  title = 'LaundrySensorSite';
  name = 'LSS';
  presses = 0;

  incrementPresses() {
    this.presses++;
  }
}
