import { Component } from '@angular/core';
import { HttpClient } from '@angular/common/http';

interface ServerMessage {
  message: string;
}


@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.scss']
})

export class AppComponent {
  title = 'LaundrySensorSite';
  name = 'LSS';
  presses = 0;
  message = '';

  constructor(private http: HttpClient) {}

  incrementPresses() {
    this.presses++;
  }

  getMessage() {
    return this.http.get<ServerMessage>('/api');
  }

  updateMessage() {
    this.getMessage()
      .subscribe((data: ServerMessage) => this.message = data.message)
  }
}
